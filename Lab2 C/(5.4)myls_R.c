/*
 * myls_R.c - "ls -R" 과 같은 결과를 내는 프로그램
 *
 *   사용법: myls_R [디렉토리 ...]      (인자가 없으면 현재 디렉토리 ".")
 *
 * 동작
 *   - 숨김 파일(이름이 '.' 으로 시작)은 ls 처럼 출력하지 않는다.
 *   - 이름은 로케일 순서(strcoll)로 정렬한다. (ls 와 같은 정렬 규칙)
 *   - 각 디렉토리는 "경로:" 머리글과 항목 목록으로 출력하고,
 *     디렉토리 사이에는 빈 줄을 하나 둔다.
 *   - 하위 디렉토리는 항목을 모두 출력한 뒤 정렬된 순서로 재귀 방문한다.
 *     심볼릭 링크는 따라가지 않는다 (lstat 사용, ls -R 과 동일).
 *   - 표준 출력이 터미널이면 ls 처럼 열(column) 단위로 정렬해 출력하고,
 *     파이프/파일이면 한 줄에 하나씩 출력한다.
 *
 * 사용하는 함수: scandir, lstat, strcoll, ioctl(TIOCGWINSZ), isatty
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <wchar.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

static int term_width = 80;
static int use_columns = 0;

static int is_visible(const struct dirent *d)
{
    return d->d_name[0] != '.';
}

static int by_name(const struct dirent **a, const struct dirent **b)
{
    return strcoll((*a)->d_name, (*b)->d_name);
}

/* 화면에서 차지하는 칸 수 (한글 등 2칸 문자를 고려) */
static int display_width(const char *s)
{
    size_t n = mbstowcs(NULL, s, 0);
    if (n == (size_t)-1)
        return (int)strlen(s);
    wchar_t *w = malloc((n + 1) * sizeof *w);
    if (w == NULL)
        return (int)strlen(s);
    mbstowcs(w, s, n + 1);
    int width = wcswidth(w, n);
    free(w);
    return width < 0 ? (int)strlen(s) : width;
}

/*
 * ls 의 "열 우선(column-major)" 배치 알고리즘 (GNU coreutils ls.c 의 방식과 같다).
 *  - 열 수를 1..max_cols 로 가정하고 각 열의 폭(이름 길이 + 구분 공백 2칸)을 계산한다.
 *  - 한 줄의 전체 길이가 터미널 폭보다 작은, 가장 많은 열 수를 선택한다.
 *  - 열 사이는 GNU ls 처럼 탭(8칸)과 공백으로 맞춘다.
 */
#define MIN_COLUMN_WIDTH 3
#define TABSIZE 8

/* GNU ls 와 같이 열 사이의 빈 공간을 탭(8칸)과 공백으로 채운다 */
static void indent(int from, int to)
{
    while (from < to) {
        if (to / TABSIZE > (from + 1) / TABSIZE) {
            putchar('\t');
            from += TABSIZE - from % TABSIZE;
        } else {
            putchar(' ');
            from++;
        }
    }
}

static void print_columns(struct dirent **names, int n)
{
    int max_idx = term_width / MIN_COLUMN_WIDTH;
    int max_cols, i, f;
    int *len, *valid, *total, **col;

    if (n == 0)
        return;
    if (max_idx < 1) max_idx = 1;
    max_cols = max_idx < n ? max_idx : n;

    len   = malloc(n * sizeof *len);
    valid = malloc(max_cols * sizeof *valid);
    total = malloc(max_cols * sizeof *total);
    col   = malloc(max_cols * sizeof *col);
    for (i = 0; i < max_cols; i++) {
        valid[i] = 1;
        total[i] = (i + 1) * MIN_COLUMN_WIDTH;
        col[i] = malloc((i + 1) * sizeof **col);
        for (int j = 0; j <= i; j++)
            col[i][j] = MIN_COLUMN_WIDTH;
    }

    for (f = 0; f < n; f++) {
        len[f] = display_width(names[f]->d_name);
        for (i = 0; i < max_cols; i++) {
            if (!valid[i])
                continue;
            int idx  = f / ((n + i) / (i + 1));
            int real = len[f] + (idx == i ? 0 : 2);
            if (col[i][idx] < real) {
                total[i] += real - col[i][idx];
                col[i][idx] = real;
                valid[i] = total[i] < term_width;
            }
        }
    }

    int cols = max_cols;
    while (cols > 1 && !valid[cols - 1])
        cols--;

    int rows = n / cols + (n % cols != 0);
    for (int r = 0; r < rows; r++) {
        int c = 0, pos = 0;
        f = r;
        for (;;) {
            int name_len = len[f];
            int col_w = col[cols - 1][c++];
            fputs(names[f]->d_name, stdout);
            f += rows;
            if (f >= n)
                break;
            indent(pos + name_len, pos + col_w);
            pos += col_w;
        }
        putchar('\n');
    }

    for (i = 0; i < max_cols; i++)
        free(col[i]);
    free(col); free(total); free(valid); free(len);
}

static void list_dir(const char *path, int *first)
{
    struct dirent **names;
    int n = scandir(path, &names, is_visible, by_name);

    if (n < 0) {
        fprintf(stderr, "myls_R: cannot open directory '%s': ", path);
        perror("");
        return;
    }
    if (!*first)
        putchar('\n');
    *first = 0;
    printf("%s:\n", path);

    if (use_columns)
        print_columns(names, n);
    else
        for (int i = 0; i < n; i++)
            puts(names[i]->d_name);

    /* 하위 디렉토리 재귀 방문 */
    size_t plen = strlen(path);
    for (int i = 0; i < n; i++) {
        size_t need = plen + strlen(names[i]->d_name) + 2;
        char *child = malloc(need);
        if (child == NULL)
            continue;
        snprintf(child, need, "%s%s%s", path,
                 (plen > 0 && path[plen - 1] == '/') ? "" : "/", names[i]->d_name);

        struct stat sb;
        if (lstat(child, &sb) == 0 && S_ISDIR(sb.st_mode))
            list_dir(child, first);
        free(child);
    }
    for (int i = 0; i < n; i++)
        free(names[i]);
    free(names);
}

int main(int argc, char *argv[])
{
    setlocale(LC_ALL, "");

    if (isatty(STDOUT_FILENO)) {
        struct winsize ws;
        use_columns = 1;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
            term_width = ws.ws_col;
        const char *env = getenv("COLUMNS");
        if (env != NULL && atoi(env) > 0)
            term_width = atoi(env);
    }

    int first = 1;
    if (argc < 2) {
        list_dir(".", &first);
    } else {
        for (int i = 1; i < argc; i++) {
            struct stat sb;
            if (lstat(argv[i], &sb) < 0) {
                fprintf(stderr, "myls_R: cannot access '%s': ", argv[i]);
                perror("");
            } else if (S_ISDIR(sb.st_mode)) {
                list_dir(argv[i], &first);
            } else {
                puts(argv[i]);
            }
        }
    }
    return 0;
}
