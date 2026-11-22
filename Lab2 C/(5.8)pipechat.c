/*
 * pipechat.c - 이름 있는 파이프(FIFO)를 이용한 1:1 텍스트 채팅
 *
 * 사용법:  터미널 1: ./pipechat A      터미널 2: ./pipechat B
 *   - 두 FIFO(/tmp/pipechat_A2B, /tmp/pipechat_B2A)를 사용해 양방향 통신한다.
 *   - select() 로 키보드(stdin)와 상대 FIFO 를 동시에 기다린다.
 *   - "/quit" 를 입력하면 상대에게 알리고 종료한다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/select.h>

#define MAXMSG 512

static void make_fifo(const char *path)
{
    if (mkfifo(path, 0666) == -1 && errno != EEXIST) {
        perror("mkfifo");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    char rpath[64], wpath[64], buf[MAXMSG];
    const char *me, *peer;
    int rfd, wfd;

    if (argc != 2 || (strcmp(argv[1], "A") && strcmp(argv[1], "B"))) {
        fprintf(stderr, "사용법: %s A|B\n", argv[0]);
        return 1;
    }
    me   = argv[1];
    peer = strcmp(me, "A") == 0 ? "B" : "A";

    snprintf(rpath, sizeof rpath, "/tmp/pipechat_%s2%s", peer, me); /* 읽기용 */
    snprintf(wpath, sizeof wpath, "/tmp/pipechat_%s2%s", me, peer); /* 쓰기용 */
    make_fifo(rpath);
    make_fifo(wpath);

    /* O_RDWR 로 열면 상대가 아직 없어도 open 이 블록되지 않는다(Linux). */
    rfd = open(rpath, O_RDWR | O_NONBLOCK);
    wfd = open(wpath, O_RDWR | O_NONBLOCK);
    if (rfd == -1 || wfd == -1) { perror("open"); return 1; }

    printf("[%s] 채팅 시작 (종료: /quit)\n", me);
    fflush(stdout);

    for (;;) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(rfd, &fds);
        if (select(rfd + 1, &fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(rfd, &fds)) {                 /* 상대가 보낸 메시지 */
            ssize_t n = read(rfd, buf, sizeof buf - 1);
            if (n > 0) {
                buf[n] = '\0';
                if (strncmp(buf, "/quit", 5) == 0) {
                    printf("[%s] 상대가 대화를 종료했습니다.\n", me);
                    break;
                }
                printf("[%s -> %s] %s", peer, me, buf);
                fflush(stdout);
            }
        }
        if (FD_ISSET(STDIN_FILENO, &fds)) {        /* 내가 입력한 메시지 */
            if (fgets(buf, sizeof buf, stdin) == NULL) {
                strcpy(buf, "/quit\n");
            }
            if (write(wfd, buf, strlen(buf)) == -1) { perror("write"); break; }
            if (strncmp(buf, "/quit", 5) == 0) {
                printf("[%s] 대화를 종료합니다.\n", me);
                break;
            }
        }
    }
    close(rfd);
    close(wfd);
    return 0;
}
