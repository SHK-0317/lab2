/*
 * ipc_demo.c - 프로세스 간 통신(IPC) 함수 사용 연습
 *
 * 강의자료 9장 '프로세스간 통신'에서 다룬 기법을 하나씩 써 본다.
 *   1. pipe          : 부모 <-> 자식 사이의 익명 파이프 (양방향은 파이프 2개)
 *   2. FIFO(mkfifo)  : 이름 있는 파이프, 파일 시스템에 이름이 남는다
 *   3. System V 메시지 큐 : msgget / msgsnd / msgrcv / msgctl (메시지 유형으로 선택 수신)
 *   4. System V 공유 메모리 + 세마포어 : shmget / shmat / shmdt / semget / semop
 *
 * 빌드: gcc -Wall -o ipc_demo ipc_demo.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>

static void title(const char *t) { printf("\n===== %s =====\n", t); }

static void die(const char *what)
{
    perror(what);
    exit(1);
}

/* ---------- 1. pipe ---------- */
static void demo_pipe(void)
{
    title("1. pipe : 부모 <-> 자식 (파이프 2개로 양방향)");
    int to_child[2], to_parent[2];
    char buf[64];

    if (pipe(to_child) < 0 || pipe(to_parent) < 0)
        die("pipe");

    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {                                   /* 자식 */
        close(to_child[1]);                           /* 안 쓰는 쪽은 닫는다 */
        close(to_parent[0]);
        ssize_t n = read(to_child[0], buf, sizeof buf - 1);
        buf[n] = '\0';
        printf("  자식: 받은 메시지 = \"%s\"\n", buf);
        const char *reply = "pong from child";
        if (write(to_parent[1], reply, strlen(reply)) < 0) _exit(1);
        _exit(0);
    }
    close(to_child[0]);
    close(to_parent[1]);
    const char *msg = "ping from parent";
    if (write(to_child[1], msg, strlen(msg)) < 0) die("write");
    ssize_t n = read(to_parent[0], buf, sizeof buf - 1);
    buf[n] = '\0';
    printf("  부모: 받은 응답   = \"%s\"\n", buf);
    close(to_child[1]);
    close(to_parent[0]);
    waitpid(pid, NULL, 0);
}

/* ---------- 2. FIFO ---------- */
static void demo_fifo(void)
{
    title("2. FIFO (이름 있는 파이프)");
    char path[64];
    char buf[128];
    snprintf(path, sizeof path, "/tmp/lab2_demo_fifo_%d", getpid());

    if (mkfifo(path, 0600) < 0)
        die("mkfifo");
    struct stat sb;
    stat(path, &sb);
    printf("  mkfifo(\"%s\") → S_ISFIFO = %s\n", path, S_ISFIFO(sb.st_mode) ? "true" : "false");

    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {                                   /* 쓰는 쪽 */
        int fd = open(path, O_WRONLY);                /* 읽는 쪽이 열릴 때까지 대기 */
        const char *m[] = {"hello ", "through ", "the FIFO\n"};
        for (int i = 0; i < 3; i++)
            if (write(fd, m[i], strlen(m[i])) < 0) _exit(1);
        close(fd);
        _exit(0);
    }
    int fd = open(path, O_RDONLY);                    /* 읽는 쪽 */
    ssize_t n, total = 0;
    while ((n = read(fd, buf + total, sizeof buf - 1 - total)) > 0)
        total += n;                                   /* 쓰는 쪽이 닫으면 0(EOF) */
    buf[total] = '\0';
    printf("  부모가 FIFO 에서 읽은 내용 = %s", buf);
    close(fd);
    waitpid(pid, NULL, 0);
    unlink(path);
}

/* ---------- 3. System V 메시지 큐 ---------- */
struct msgbuf_t {
    long mtype;
    char mtext[64];
};

static void demo_msgq(void)
{
    title("3. System V 메시지 큐 (메시지 유형으로 선택 수신)");
    int qid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
    if (qid < 0) die("msgget");

    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {                                   /* 송신자: 유형 2 를 먼저, 유형 1 을 나중에 */
        struct msgbuf_t m;
        m.mtype = 2; strcpy(m.mtext, "두 번째 유형(2) 메시지");
        msgsnd(qid, &m, strlen(m.mtext) + 1, 0);
        m.mtype = 1; strcpy(m.mtext, "첫 번째 유형(1) 메시지");
        msgsnd(qid, &m, strlen(m.mtext) + 1, 0);
        m.mtype = 3; strcpy(m.mtext, "세 번째 유형(3) 메시지");
        msgsnd(qid, &m, strlen(m.mtext) + 1, 0);
        _exit(0);
    }
    waitpid(pid, NULL, 0);                            /* 3 개가 모두 큐에 들어간 상태 */

    struct msqid_ds info;
    msgctl(qid, IPC_STAT, &info);
    printf("  큐에 쌓인 메시지 수 (msg_qnum) = %lu\n", (unsigned long)info.msg_qnum);

    struct msgbuf_t r;
    msgrcv(qid, &r, sizeof r.mtext, 1, 0);            /* msgtyp = 1 : 유형 1 만 */
    printf("  msgrcv(msgtyp=1) → [%ld] %s\n", r.mtype, r.mtext);
    msgrcv(qid, &r, sizeof r.mtext, 0, 0);            /* msgtyp = 0 : 큐의 맨 앞 */
    printf("  msgrcv(msgtyp=0) → [%ld] %s\n", r.mtype, r.mtext);
    msgrcv(qid, &r, sizeof r.mtext, -3, 0);           /* 음수 : 절대값 이하 중 가장 작은 유형 */
    printf("  msgrcv(msgtyp=-3) → [%ld] %s\n", r.mtype, r.mtext);

    if (msgctl(qid, IPC_RMID, NULL) < 0)
        die("msgctl(IPC_RMID)");
    printf("  msgctl(IPC_RMID) 로 큐 삭제\n");
}

/* ---------- 4. System V 공유 메모리 + 세마포어 ---------- */
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static void sem_op1(int semid, short op)
{
    struct sembuf sb = {0, op, 0};
    if (semop(semid, &sb, 1) < 0)
        die("semop");
}

static void demo_shm_sem(void)
{
    title("4. System V 공유 메모리 + 세마포어(동기화)");
    int shmid = shmget(IPC_PRIVATE, 256, IPC_CREAT | 0600);
    int semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0600);
    if (shmid < 0) die("shmget");
    if (semid < 0) die("semget");

    union semun arg;
    arg.val = 0;                                      /* 0 = "자료가 아직 준비되지 않음" */
    semctl(semid, 0, SETVAL, arg);

    pid_t pid = fork();
    if (pid < 0) die("fork");
    if (pid == 0) {                                   /* 쓰는 쪽(자식) */
        char *p = shmat(shmid, NULL, 0);
        if (p == (void *)-1) die("shmat");
        usleep(200 * 1000);                           /* 일부러 늦게 써서 부모가 기다리는 모습을 보임 */
        snprintf(p, 256, "공유 메모리에 쓴 문자열 (writer pid=%d)", getpid());
        printf("  자식: 공유 메모리에 기록 완료 → 세마포어 V(+1)\n");
        sem_op1(semid, +1);
        shmdt(p);
        _exit(0);
    }
    char *p = shmat(shmid, NULL, 0);                  /* 읽는 쪽(부모) */
    if (p == (void *)-1) die("shmat");
    printf("  부모: 세마포어 P(-1) 로 자식의 기록을 기다리는 중...\n");
    sem_op1(semid, -1);                               /* 값이 0 이면 +1 될 때까지 대기 */
    printf("  부모: 읽은 내용 = \"%s\"\n", p);

    struct shmid_ds info;
    shmctl(shmid, IPC_STAT, &info);
    printf("  shm_segsz = %zu 바이트, 현재 부착 수(shm_nattch) = %lu\n",
           (size_t)info.shm_segsz, (unsigned long)info.shm_nattch);
    shmdt(p);
    waitpid(pid, NULL, 0);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, arg);
    printf("  shmctl/semctl(IPC_RMID) 로 자원 삭제\n");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    demo_pipe();
    demo_fifo();
    demo_msgq();
    demo_shm_sem();
    printf("\n모든 데모 종료\n");
    return 0;
}
