/*
 * proc_signal.c - 프로세스 및 시그널 관련 함수 사용 연습
 *
 * 사용하는 함수 (강의자료 8장 '프로세스와 시그널')
 *   fork, exec(execlp), wait/waitpid, exit, getpid/getppid,
 *   sigaction, sigemptyset/sigaddset/sigfillset, sigprocmask, sigpending,
 *   kill, raise, alarm, pause
 *
 * 각 데모는 번호를 붙여 차례로 실행되며, 출력 순서가 매번 같도록
 * 파이프(동기화용)와 wait 를 사용했다.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* ---------- 도우미 ---------- */
static void title(const char *t) { printf("\n===== %s =====\n", t); }

/* 자식의 종료 상태를 해석해서 출력 */
static void report_status(pid_t pid, int status)
{
    if (WIFEXITED(status))
        printf("  부모: 자식 %d 종료 - exit status = %d\n", pid, WEXITSTATUS(status));
    else if (WIFSIGNALED(status))
        printf("  부모: 자식 %d 시그널로 종료 - %d (%s)\n", pid,
               WTERMSIG(status), strsignal(WTERMSIG(status)));
}

/* /proc/<pid>/stat 의 상태 문자(R, S, Z ...) */
static char proc_state(pid_t pid)
{
    char path[64], buf[256];
    snprintf(path, sizeof path, "/proc/%d/stat", pid);
    FILE *f = fopen(path, "r");
    if (f == NULL) return '?';
    if (fgets(buf, sizeof buf, f) == NULL) { fclose(f); return '?'; }
    fclose(f);
    char *p = strrchr(buf, ')');
    return p ? p[2] : '?';
}

/* ---------- 1. fork / exec / wait ---------- */
static void demo_fork_exec_wait(void)
{
    title("1. fork + exec + wait");
    printf("  부모 pid=%d, ppid=%d\n", getpid(), getppid());

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {                         /* 자식: 새 프로그램으로 교체 */
        printf("  자식 pid=%d, ppid=%d : execlp 로 echo 실행\n", getpid(), getppid());
        execlp("echo", "echo", "    (echo 가 출력) hello from exec", (char *)NULL);
        perror("execlp");                   /* 성공하면 여기로 돌아오지 않는다 */
        _exit(127);
    }
    int status;
    waitpid(pid, &status, 0);
    report_status(pid, status);

    pid = fork();                           /* 종료 코드 전달 확인 */
    if (pid == 0)
        exit(5);
    waitpid(pid, &status, 0);
    report_status(pid, status);
}

/* ---------- 2. 좀비 프로세스 ---------- */
static void demo_zombie(void)
{
    title("2. 좀비(zombie) 프로세스");
    pid_t pid = fork();
    if (pid == 0)
        _exit(0);                           /* 자식은 곧바로 종료 */
    usleep(200 * 1000);                     /* 부모가 wait 하지 않고 잠시 기다림 */
    printf("  wait 하기 전 자식 %d 의 상태 = %c  (Z = zombie)\n", pid, proc_state(pid));
    waitpid(pid, NULL, 0);
    printf("  wait 한 뒤   자식 %d 의 상태 = %c  (? = 완전히 사라짐)\n", pid, proc_state(pid));
}

/* ---------- 3. 시그널 핸들러 + kill ---------- */
static volatile sig_atomic_t usr1_count = 0;

static void on_usr1(int signo)
{
    (void)signo;
    usr1_count++;
    const char msg[] = "    자식: SIGUSR1 핸들러 실행\n";
    ssize_t w = write(STDOUT_FILENO, msg, sizeof msg - 1);
    (void)w;
}

static void demo_kill_handler(void)
{
    title("3. sigaction 핸들러 + kill");
    int ready[2];
    if (pipe(ready) < 0) { perror("pipe"); exit(1); }

    pid_t pid = fork();
    if (pid == 0) {
        struct sigaction sa;
        sa.sa_handler = on_usr1;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGUSR1, &sa, NULL);
        if (write(ready[1], "r", 1) < 0) _exit(1);   /* 핸들러 등록 완료를 부모에게 알림 */
        while (usr1_count < 3)
            pause();                        /* 시그널이 올 때까지 대기 */
        printf("  자식: SIGUSR1 을 %d 번 받고 종료\n", (int)usr1_count);
        exit(0);
    }
    char c;
    if (read(ready[0], &c, 1) < 0) perror("read");
    for (int i = 1; i <= 3; i++) {
        printf("  부모: kill(%d, SIGUSR1) %d 번째 전송\n", pid, i);
        kill(pid, SIGUSR1);
        usleep(150 * 1000);
    }
    int status;
    waitpid(pid, &status, 0);
    report_status(pid, status);
}

/* ---------- 4. 시그널로 자식 종료시키기 (기본 동작 / 무시) ---------- */
static void demo_terminate(void)
{
    title("4. 시그널의 기본 동작과 SIG_IGN");
    int ready[2];
    if (pipe(ready) < 0) { perror("pipe"); exit(1); }

    pid_t pid = fork();
    if (pid == 0) {                         /* 자식 A: SIGTERM 기본 동작 (종료) */
        if (write(ready[1], "r", 1) < 0) _exit(1);
        for (;;) pause();
    }
    char c;
    if (read(ready[0], &c, 1) < 0) perror("read");
    printf("  부모: 자식 %d 에게 SIGTERM (기본 동작 = 종료)\n", pid);
    kill(pid, SIGTERM);
    int status;
    waitpid(pid, &status, 0);
    report_status(pid, status);

    pid = fork();
    if (pid == 0) {                         /* 자식 B: SIGTERM 을 무시 */
        struct sigaction sa;
        sa.sa_handler = SIG_IGN;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGTERM, &sa, NULL);
        if (write(ready[1], "r", 1) < 0) _exit(1);
        for (;;) pause();
    }
    if (read(ready[0], &c, 1) < 0) perror("read");
    printf("  부모: 자식 %d 에게 SIGTERM (SIG_IGN 이므로 살아 있음)\n", pid);
    kill(pid, SIGTERM);
    usleep(200 * 1000);
    printf("  부모: 200ms 후 자식 상태 = %c (S = sleeping, 살아 있음)\n", proc_state(pid));
    printf("  부모: 자식 %d 에게 SIGKILL (무시·차단 불가)\n", pid);
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    report_status(pid, status);
}

/* ---------- 5. 시그널 차단 (sigprocmask) ---------- */
static void on_usr2(int signo)
{
    (void)signo;
    const char msg[] = "    핸들러: SIGUSR2 처리됨\n";
    ssize_t w = write(STDOUT_FILENO, msg, sizeof msg - 1);
    (void)w;
}

static void demo_mask(void)
{
    title("5. sigprocmask 로 시그널 차단 / sigpending / raise");
    struct sigaction sa;
    sa.sa_handler = on_usr2;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR2, &sa, NULL);

    sigset_t set, pending;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);

    sigprocmask(SIG_BLOCK, &set, NULL);
    printf("  SIGUSR2 차단 후 raise(SIGUSR2) 호출\n");
    raise(SIGUSR2);
    sigpending(&pending);
    printf("  sigpending: SIGUSR2 가 대기 중인가? %s\n",
           sigismember(&pending, SIGUSR2) ? "예 (아직 전달 안 됨)" : "아니오");
    printf("  차단 해제 (SIG_UNBLOCK)\n");
    sigprocmask(SIG_UNBLOCK, &set, NULL);   /* 해제하는 순간 대기 중이던 시그널 전달 */
    printf("  차단 해제 후 대기 시그널 있나? %s\n",
           (sigpending(&pending), sigismember(&pending, SIGUSR2)) ? "예" : "아니오");
}

/* ---------- 6. alarm ---------- */
static volatile sig_atomic_t alarmed = 0;
static void on_alarm(int signo) { (void)signo; alarmed = 1; }

static void demo_alarm(void)
{
    title("6. alarm + pause");
    struct sigaction sa;
    sa.sa_handler = on_alarm;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);

    printf("  alarm(1) 설정 후 pause() 로 대기\n");
    alarm(1);
    pause();
    printf("  SIGALRM 수신 → alarmed = %d\n", (int)alarmed);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);       /* fork 후 버퍼 중복 출력을 막는다 */
    demo_fork_exec_wait();
    demo_zombie();
    demo_kill_handler();
    demo_terminate();
    demo_mask();
    demo_alarm();
    printf("\n모든 데모 종료\n");
    return 0;
}
