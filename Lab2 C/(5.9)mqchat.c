/*
 * mqchat.c - POSIX 메시지 큐를 이용한 1:1 텍스트 채팅
 *
 * 사용법:  터미널 1: ./mqchat A      터미널 2: ./mqchat B
 *   - 큐 /mqchat_A2B, /mqchat_B2A 를 사용한다. (mq_open/mq_send/mq_receive)
 *   - 수신은 별도 쓰레드가, 송신은 메인 쓰레드가 맡는다.
 *   - "/quit" 를 입력하면 상대에게 알리고 종료한다. 종료 시 자신이 읽던 큐는 mq_unlink.
 *   - 컴파일: gcc -o mqchat mqchat.c -lrt -lpthread
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <mqueue.h>
#include <sys/stat.h>

#define MAXMSG  512
#define PRIO_NORMAL 1
#define PRIO_QUIT   9           /* 종료 메시지는 높은 우선순위 */

static mqd_t rq, wq;
static const char *me, *peer;
static char rname[64], wname[64];
static volatile int done = 0;

static void *receiver(void *arg)
{
    char buf[MAXMSG + 1];
    unsigned prio;
    (void)arg;
    while (!done) {
        ssize_t n = mq_receive(rq, buf, MAXMSG + 1, &prio);
        if (n == -1) { if (errno == EINTR) continue; break; }
        buf[n] = '\0';
        if (prio == PRIO_QUIT) {
            printf("[%s] 상대가 대화를 종료했습니다. (Enter 를 누르세요)\n", me);
            fflush(stdout);
            done = 1;
            break;
        }
        printf("[%s -> %s] %s", peer, me, buf);
        fflush(stdout);
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    struct mq_attr attr;
    pthread_t tid;
    char buf[MAXMSG];

    if (argc != 2 || (strcmp(argv[1], "A") && strcmp(argv[1], "B"))) {
        fprintf(stderr, "사용법: %s A|B\n", argv[0]);
        return 1;
    }
    me   = argv[1];
    peer = strcmp(me, "A") == 0 ? "B" : "A";
    snprintf(rname, sizeof rname, "/mqchat_%s2%s", peer, me);
    snprintf(wname, sizeof wname, "/mqchat_%s2%s", me, peer);

    memset(&attr, 0, sizeof attr);
    attr.mq_maxmsg  = 10;
    attr.mq_msgsize = MAXMSG;

    rq = mq_open(rname, O_CREAT | O_RDONLY, 0666, &attr);
    wq = mq_open(wname, O_CREAT | O_WRONLY, 0666, &attr);
    if (rq == (mqd_t)-1 || wq == (mqd_t)-1) { perror("mq_open"); return 1; }

    pthread_create(&tid, NULL, receiver, NULL);
    printf("[%s] 채팅 시작 (종료: /quit)\n", me);
    fflush(stdout);

    while (!done && fgets(buf, sizeof buf, stdin) != NULL) {
        if (strncmp(buf, "/quit", 5) == 0) {
            mq_send(wq, "/quit", 5, PRIO_QUIT);
            printf("[%s] 대화를 종료합니다.\n", me);
            break;
        }
        if (mq_send(wq, buf, strlen(buf), PRIO_NORMAL) == -1) {
            perror("mq_send");
            break;
        }
    }
    if (!done && feof(stdin))                    /* 입력 EOF 도 종료로 처리 */
        mq_send(wq, "/quit", 5, PRIO_QUIT);

    done = 1;
    pthread_cancel(tid);                         /* mq_receive 에서 대기 중인 쓰레드 종료 */
    pthread_join(tid, NULL);
    mq_close(rq);
    mq_close(wq);
    mq_unlink(rname);                            /* 내가 읽는 큐는 내가 정리 */
    return 0;
}
