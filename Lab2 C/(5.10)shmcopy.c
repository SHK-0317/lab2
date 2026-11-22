/*
 * shmcopy.c - POSIX 공유 메모리(shm_open + mmap)를 이용한 파일 복사
 *
 * 사용법: ./shmcopy <원본파일> <복사본파일>
 *   1) 부모: 원본 파일을 mmap 으로 읽기 전용 매핑, 공유 메모리 객체(/shmcopy_buf)를
 *            파일 크기만큼 ftruncate 후 mmap(MAP_SHARED)하여 내용을 복사한다.
 *   2) 자식: fork 후 공유 메모리를 매핑하여 목적 파일에 write 로 저장한다.
 *   부모가 쓰기를 마치면 파이프로 알려 자식이 저장을 시작하도록 동기화한다.
 *   - 컴파일: gcc -o shmcopy shmcopy.c -lrt
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define SHM_NAME "/shmcopy_buf"

static void die(const char *msg) { perror(msg); shm_unlink(SHM_NAME); exit(1); }

int main(int argc, char *argv[])
{
    int src, shmfd, sync_pipe[2];
    struct stat st;
    pid_t pid;

    if (argc != 3) {
        fprintf(stderr, "사용법: %s <원본> <복사본>\n", argv[0]);
        return 1;
    }
    if ((src = open(argv[1], O_RDONLY)) == -1) die(argv[1]);
    if (fstat(src, &st) == -1) die("fstat");
    if (!S_ISREG(st.st_mode)) { fprintf(stderr, "일반 파일이 아닙니다.\n"); return 1; }

    if (st.st_size == 0) {                       /* 빈 파일: mmap 길이 0 은 오류 */
        int d = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
        if (d == -1) die(argv[2]);
        close(d);
        printf("0 바이트 복사 완료\n");
        return 0;
    }
    size_t size = (size_t)st.st_size;

    shmfd = shm_open(SHM_NAME, O_CREAT | O_RDWR | O_TRUNC, 0600);
    if (shmfd == -1) die("shm_open");
    if (ftruncate(shmfd, (off_t)size) == -1) die("ftruncate");

    if (pipe(sync_pipe) == -1) die("pipe");
    if ((pid = fork()) == -1) die("fork");

    if (pid == 0) {                              /* 자식: 공유 메모리 -> 목적 파일 */
        char token;
        close(sync_pipe[1]);
        if (read(sync_pipe[0], &token, 1) != 1) _exit(2);   /* 부모가 끝날 때까지 대기 */
        char *shm = mmap(NULL, size, PROT_READ, MAP_SHARED, shmfd, 0);
        if (shm == MAP_FAILED) { perror("child mmap"); _exit(1); }
        int dst = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, st.st_mode & 0777);
        if (dst == -1) { perror(argv[2]); _exit(1); }
        size_t off = 0;
        while (off < size) {
            ssize_t w = write(dst, shm + off, size - off);
            if (w == -1) { perror("write"); _exit(1); }
            off += (size_t)w;
        }
        close(dst);
        munmap(shm, size);
        _exit(0);
    }

    /* 부모: 원본 파일 -> 공유 메모리 */
    close(sync_pipe[0]);
    char *in  = mmap(NULL, size, PROT_READ, MAP_PRIVATE, src, 0);
    char *shm = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
    if (in == MAP_FAILED || shm == MAP_FAILED) die("mmap");
    memcpy(shm, in, size);
    if (msync(shm, size, MS_SYNC) == -1) die("msync");
    if (write(sync_pipe[1], "1", 1) != 1) die("sync write");   /* 자식에게 신호 */
    close(sync_pipe[1]);

    int status;
    waitpid(pid, &status, 0);
    munmap(in, size);
    munmap(shm, size);
    close(src);
    close(shmfd);
    shm_unlink(SHM_NAME);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "복사 실패\n");
        return 1;
    }
    printf("%zu 바이트 복사 완료: %s -> %s\n", size, argv[1], argv[2]);
    return 0;
}
