/*
 * notify_client.c - D-Bus(sd-bus) 알림 클라이언트
 *   ./notify_client send <제목> <내용>   서버의 Notify 메서드를 호출하고 id 를 출력
 *   ./notify_client count                서버의 Count 프로퍼티를 읽어 출력
 *   ./notify_client listen [초]          Notified 시그널을 지정 시간(기본 5초) 동안 수신
 *   컴파일: gcc -o notify_client notify_client.c $(pkg-config --cflags --libs libsystemd)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#define BUS_NAME  "kr.lab2.Notifier"
#define OBJ_PATH  "/kr/lab2/Notifier"
#define IFACE     "kr.lab2.Notifier"

static int on_notified(sd_bus_message *m, void *userdata, sd_bus_error *err)
{
    unsigned id; const char *t, *b;
    (void)userdata; (void)err;
    if (sd_bus_message_read(m, "uss", &id, &t, &b) < 0) return 0;
    printf("[클라이언트] 시그널 수신 #%u: [%s] %s\n", id, t, b);
    fflush(stdout);
    return 0;
}

int main(int argc, char *argv[])
{
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *reply = NULL;
    sd_bus *bus = NULL;
    int r = 0;

    if (argc < 2) goto usage;
    if ((r = sd_bus_open_user(&bus)) < 0) {
        fprintf(stderr, "세션 버스 연결 실패: %s\n", strerror(-r));
        return 1;
    }

    if (strcmp(argv[1], "send") == 0 && argc == 4) {
        unsigned id;
        r = sd_bus_call_method(bus, BUS_NAME, OBJ_PATH, IFACE, "Notify",
                               &error, &reply, "ss", argv[2], argv[3]);
        if (r < 0) { fprintf(stderr, "호출 실패: %s\n", error.message ? error.message : strerror(-r)); goto out; }
        if ((r = sd_bus_message_read(reply, "u", &id)) < 0) goto out;
        printf("[클라이언트] 서버가 알림 번호 %u 를 돌려주었습니다.\n", id);
    } else if (strcmp(argv[1], "count") == 0) {
        uint32_t n;
        r = sd_bus_get_property_trivial(bus, BUS_NAME, OBJ_PATH, IFACE, "Count", &error, 'u', &n);
        if (r < 0) { fprintf(stderr, "프로퍼티 읽기 실패: %s\n", error.message ? error.message : strerror(-r)); goto out; }
        printf("[클라이언트] 서버가 지금까지 받은 알림 수: %u\n", n);
    } else if (strcmp(argv[1], "listen") == 0) {
        int secs = argc > 2 ? atoi(argv[2]) : 5;
        sd_bus_slot *slot = NULL;
        r = sd_bus_match_signal(bus, &slot, BUS_NAME, OBJ_PATH, IFACE, "Notified", on_notified, NULL);
        if (r < 0) { fprintf(stderr, "match 등록 실패: %s\n", strerror(-r)); goto out; }
        printf("[클라이언트] %d초 동안 Notified 시그널을 기다립니다...\n", secs);
        fflush(stdout);
        sd_bus_flush(bus);
        /* 단순 구현: 1초 단위로 처리하며 반복 */
        for (int i = 0; i < secs; i++) {
            for (;;) { r = sd_bus_process(bus, NULL); if (r <= 0) break; }
            sd_bus_wait(bus, 1000000);
        }
        for (;;) { r = sd_bus_process(bus, NULL); if (r <= 0) break; }
        sd_bus_slot_unref(slot);
        r = 0;
    } else goto usage;

out:
    sd_bus_error_free(&error);
    sd_bus_message_unref(reply);
    sd_bus_unref(bus);
    return r < 0 ? 1 : 0;
usage:
    fprintf(stderr, "사용법: %s send <제목> <내용> | count | listen [초]\n", argv[0]);
    return 1;
}
