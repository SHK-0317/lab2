/*
 * file_ops.c - 파일 및 디렉토리 관련 시스템 호출 사용 연습
 *
 * 사용하는 함수 (강의자료 7장 '파일 처리'):
 *   getcwd, chdir, mkdir, rmdir, opendir/readdir/closedir,
 *   open, creat, read, write, lseek, close, dup2,
 *   stat/lstat, access, umask, link, symlink, readlink, unlink, rename
 *
 * 현재 디렉토리 아래에 demo_dir 를 만들어 실험하고, 마지막에 모두 정리한다.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#define DEMO_DIR "demo_dir"

static void step(const char *title) { printf("\n[%s]\n", title); }

static void die(const char *what)
{
    perror(what);
    exit(1);
}

/* st_mode 의 파일 유형을 문자열로 */
static const char *type_str(mode_t m)
{
    if (S_ISREG(m))  return "regular file";
    if (S_ISDIR(m))  return "directory";
    if (S_ISCHR(m))  return "character special file";
    if (S_ISBLK(m))  return "block special file";
    if (S_ISFIFO(m)) return "FIFO";
    if (S_ISLNK(m))  return "symbolic link";
    if (S_ISSOCK(m)) return "socket";
    return "unknown";
}

/* 권한 비트를 rwxr-xr-x 형태로 */
static void perm_str(mode_t m, char out[10])
{
    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++)
        out[i] = (m & (1 << (8 - i))) ? rwx[i] : '-';
    out[9] = '\0';
}

static void show_stat(const char *path)
{
    struct stat sb;
    char perm[10], tbuf[64];

    if (lstat(path, &sb) < 0)
        die("lstat");
    perm_str(sb.st_mode, perm);
    strftime(tbuf, sizeof tbuf, "%Y-%m-%d %H:%M:%S", localtime(&sb.st_mtime));
    printf("  %-18s type=%-14s mode=%04o (%s) links=%lu size=%ld mtime=%s\n",
           path, type_str(sb.st_mode), sb.st_mode & 07777, perm,
           (unsigned long)sb.st_nlink, (long)sb.st_size, tbuf);
}

static void list_dir(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *e;
    int names = 0;

    if (d == NULL)
        die("opendir");
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        printf("  %s%s\n", e->d_name, e->d_type == DT_DIR ? "/" : "");
        names++;
    }
    if (names == 0)
        printf("  (비어 있음)\n");
    closedir(d);
}

int main(void)
{
    char cwd[PATH_MAX], buf[256];
    int fd;
    ssize_t n;

    if (access(DEMO_DIR, F_OK) == 0) {
        fprintf(stderr, "%s 가 이미 있습니다. 지우고 다시 실행하세요.\n", DEMO_DIR);
        return 1;
    }

    step("1. getcwd / mkdir / chdir");
    if (getcwd(cwd, sizeof cwd) == NULL) die("getcwd");
    printf("  현재 작업 디렉토리 = %s\n", cwd);
    if (mkdir(DEMO_DIR, 0755) < 0) die("mkdir");
    printf("  mkdir(\"%s\", 0755) 성공\n", DEMO_DIR);
    if (chdir(DEMO_DIR) < 0) die("chdir");
    if (getcwd(cwd, sizeof cwd) == NULL) die("getcwd");
    printf("  chdir 후 작업 디렉토리 = %s\n", cwd);

    step("2. open(O_CREAT) + write : 파일 만들기");
    if ((fd = open("a.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) die("open");
    const char *line1 = "Hello, Linux file I/O!\n";
    n = write(fd, line1, strlen(line1));
    printf("  write() = %zd 바이트 (fd = %d)\n", n, fd);
    close(fd);
    show_stat("a.txt");

    step("3. O_APPEND 로 이어 쓰기");
    if ((fd = open("a.txt", O_WRONLY | O_APPEND)) < 0) die("open");
    const char *line2 = "second line appended.\n";
    n = write(fd, line2, strlen(line2));
    printf("  write() = %zd 바이트\n", n);
    close(fd);

    step("4. read + lseek : 읽기, 파일 크기 구하기, 임의 접근");
    if ((fd = open("a.txt", O_RDONLY)) < 0) die("open");
    off_t size = lseek(fd, 0, SEEK_END);
    printf("  lseek(fd, 0, SEEK_END) = %ld  (파일 크기)\n", (long)size);
    lseek(fd, 7, SEEK_SET);
    n = read(fd, buf, 5);
    buf[n] = '\0';
    printf("  lseek(fd, 7, SEEK_SET) 후 5바이트 read = \"%s\"\n", buf);
    lseek(fd, 0, SEEK_SET);
    n = read(fd, buf, sizeof buf - 1);
    buf[n] = '\0';
    printf("  전체 내용(%zd 바이트):\n%s", n, buf);
    close(fd);

    step("5. stat / access");
    show_stat("a.txt");
    const char *names[4] = {"F_OK(존재)", "R_OK(읽기)", "W_OK(쓰기)", "X_OK(실행)"};
    int modes[4] = {F_OK, R_OK, W_OK, X_OK};
    for (int i = 0; i < 4; i++)
        printf("  access(\"a.txt\", %s) = %s\n", names[i],
               access("a.txt", modes[i]) == 0 ? "OK" : strerror(errno));

    step("6. umask : 파일 생성 권한 마스크");
    mode_t old = umask(0);
    if ((fd = creat("mask0.txt", 0666)) < 0) die("creat");
    close(fd);
    umask(0077);
    if ((fd = creat("mask077.txt", 0666)) < 0) die("creat");
    close(fd);
    umask(old);
    printf("  (umask 0 일 때)    "); show_stat("mask0.txt");
    printf("  (umask 0077 일 때) "); show_stat("mask077.txt");

    step("7. link / symlink / readlink / lstat");
    if (link("a.txt", "hard.txt") < 0) die("link");
    if (symlink("a.txt", "sym.txt") < 0) die("symlink");
    n = readlink("sym.txt", buf, sizeof buf - 1);
    buf[n] = '\0';
    printf("  readlink(\"sym.txt\") = \"%s\"\n", buf);
    show_stat("a.txt");     /* 하드 링크 때문에 links=2 */
    show_stat("hard.txt");
    show_stat("sym.txt");

    step("8. mkdir 로 하위 디렉토리 + rename(mv)");
    if (mkdir("sub", 0755) < 0) die("mkdir");
    if (rename("hard.txt", "sub/moved.txt") < 0) die("rename");
    printf("  rename(\"hard.txt\", \"sub/moved.txt\") 성공\n");

    step("9. dup2 로 표준 출력 재지향");
    fflush(stdout);
    int saved = dup(STDOUT_FILENO);
    if ((fd = open("redirect.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) die("open");
    dup2(fd, STDOUT_FILENO);
    close(fd);
    printf("이 줄은 화면이 아니라 redirect.txt 로 갑니다.\n");
    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);
    if ((fd = open("redirect.txt", O_RDONLY)) < 0) die("open");
    n = read(fd, buf, sizeof buf - 1);
    buf[n] = '\0';
    close(fd);
    printf("  redirect.txt 내용: %s", buf);

    step("10. opendir / readdir : 디렉토리 목록");
    list_dir(".");
    printf("  -- sub --\n");
    list_dir("sub");

    step("11. 삭제: unlink / rmdir (비어 있지 않으면 실패)");
    if (rmdir("sub") < 0)
        printf("  rmdir(\"sub\") 실패: %s\n", strerror(errno));
    const char *files[] = {"a.txt", "sym.txt", "mask0.txt", "mask077.txt",
                           "redirect.txt", "sub/moved.txt"};
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++)
        if (unlink(files[i]) < 0) die("unlink");
    printf("  unlink 로 파일 %zu 개 삭제\n", sizeof files / sizeof files[0]);
    if (rmdir("sub") < 0) die("rmdir");
    printf("  rmdir(\"sub\") 성공\n");
    if (chdir("..") < 0) die("chdir");
    if (rmdir(DEMO_DIR) < 0) die("rmdir");
    printf("  rmdir(\"%s\") 성공 → 정리 완료\n", DEMO_DIR);
    return 0;
}
