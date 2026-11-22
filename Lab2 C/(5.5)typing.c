/*
 * typing.c - 타자 연습 프로그램
 *
 * 몇 개의 문장을 차례로 보여 주고 사용자가 따라 치게 한다.
 * 문장마다, 그리고 전체에 대해 다음을 측정한다.
 *   - 잘못 타이핑한 횟수 (틀린 글자를 누를 때마다 1회, 지워도 취소되지 않음)
 *   - 걸린 시간 (첫 글자를 누른 순간부터 Enter 까지)
 *   - 분당 타자수 (타/분 = 누른 글자 수 / 걸린 분),  참고용으로 WPM(=타/분 ÷ 5)
 *
 * 구현 포인트 (강의자료 7장 '터미널 처리 - 비정규 모드')
 *   - tcgetattr/tcsetattr 로 ICANON, ECHO 를 끄고 VMIN=1, VTIME=0 으로 설정하여
 *     Enter 를 기다리지 않고 글자 하나씩 즉시 읽는다.
 *   - 맞은 글자는 그대로, 틀린 글자는 '*' 로 화면에 보여 준다.
 *   - Backspace 로 지울 수 있다.
 *   - 종료하거나 Ctrl-C 를 눌러도 터미널 설정을 원래대로 복구한다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>

static const char *sentences[] = {
    "The quick brown fox jumps over the lazy dog.",
    "Linux is a free and open source operating system.",
    "A journey of a thousand miles begins with a single step.",
};
#define N_SENTENCES (int)(sizeof sentences / sizeof sentences[0])

static struct termios saved_attr;
static int attr_saved = 0;

static void restore_terminal(void)
{
    if (attr_saved)
        tcsetattr(STDIN_FILENO, TCSANOW, &saved_attr);
}

static void on_sigint(int signo)
{
    (void)signo;
    restore_terminal();
    const char msg[] = "\n중단되었습니다.\n";
    ssize_t w = write(STDOUT_FILENO, msg, sizeof msg - 1);
    (void)w;
    _exit(130);
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void)
{
    struct termios raw;
    struct sigaction sa;

    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "typing: 표준 입력이 터미널이 아닙니다.\n");
        return 1;
    }
    if (tcgetattr(STDIN_FILENO, &saved_attr) < 0) {
        perror("tcgetattr");
        return 1;
    }
    attr_saved = 1;
    atexit(restore_terminal);

    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    raw = saved_attr;
    raw.c_lflag &= ~(ICANON | ECHO);   /* 비정규 모드 + 에코 끄기 */
    raw.c_cc[VMIN] = 1;                /* 1글자가 들어오면 read 반환 */
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0) {
        perror("tcsetattr");
        return 1;
    }

    printf("=== 타자 연습 (문장 %d개) ===\n", N_SENTENCES);
    printf("문장을 그대로 따라 친 뒤 Enter 를 누르세요. 틀린 글자는 * 로 표시됩니다.\n");

    int total_chars = 0, total_errors = 0;
    double total_time = 0.0;

    for (int s = 0; s < N_SENTENCES; s++) {
        const char *text = sentences[s];
        size_t len = strlen(text);
        size_t cnt = 0;
        int errors = 0, keystrokes = 0;
        double t0 = 0.0;
        char ch;

        printf("\n[%d/%d] %s\n      ", s + 1, N_SENTENCES, text);
        fflush(stdout);

        while (read(STDIN_FILENO, &ch, 1) == 1 && ch != '\n' && ch != '\r') {
            if (ch == 0x7f || ch == '\b') {            /* Backspace */
                if (cnt > 0) {
                    cnt--;
                    fputs("\b \b", stdout);
                    fflush(stdout);
                }
                continue;
            }
            if ((unsigned char)ch < 0x20 || cnt >= len)  /* 제어문자/넘치는 입력 무시 */
                continue;
            if (keystrokes == 0)
                t0 = now_sec();                         /* 첫 글자에서 시간 측정 시작 */
            keystrokes++;
            if (ch == text[cnt]) {
                putchar(ch);
            } else {
                putchar('*');
                errors++;
            }
            cnt++;
            fflush(stdout);
        }
        double elapsed = keystrokes ? now_sec() - t0 : 0.0;
        int unfinished = (int)(len - cnt);     /* 다 치지 못하고 Enter 를 누른 글자 수 */
        errors += unfinished;

        double cpm = elapsed > 0 ? cnt / (elapsed / 60.0) : 0.0;
        printf("\n      -> 걸린 시간 %.2f초, 오타 %d회%s, 분당 타자수 %.0f타 (%.1f WPM)\n",
               elapsed, errors,
               unfinished ? "(미입력 글자 포함)" : "", cpm, cpm / 5.0);

        total_chars += (int)cnt;
        total_errors += errors;
        total_time += elapsed;
    }

    double avg_cpm = total_time > 0 ? total_chars / (total_time / 60.0) : 0.0;
    printf("\n=== 결과 ===\n");
    printf("입력한 글자 수 : %d\n", total_chars);
    printf("잘못 친 횟수   : %d\n", total_errors);
    printf("총 걸린 시간   : %.2f초\n", total_time);
    printf("평균 분당 타자수: %.0f타/분 (%.1f WPM)\n", avg_cpm, avg_cpm / 5.0);
    return 0;
}
