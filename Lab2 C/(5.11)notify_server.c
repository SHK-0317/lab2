/*
 * notify_server.c - D-Bus(sd-bus) 알림 서버
 *   버스 이름 : kr.lab2.Notifier      오브젝트 경로 : /kr/lab2/Notifier
 *   인터페이스: kr.lab2.Notifier
 *     메서드  : Notify(s title, s body) -> u id   (알림을 받아 화면에 출력, 일련번호 반환)
 *     시그널  : Notified(u id, s title, s body)   (알림이 도착할 때마다 방송)
 *     프로퍼티: Count (u, 읽기 전용)              (지금까지 받은 알림 수)
 *   컴파일: gcc -o notify_server notify_server.c $(pkg-config --cflags --libs libsystemd)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <systemd/sd-bus.h>

#define BUS_NAME  "kr.lab2.Notifier"
#define OBJ_PATH  "/kr/lab2/Notifier"
#define IFACE     "kr.lab2.Notifier"

static unsigned counter = 0;

static int method_notify(sd_bus_message *m, void *userdata, sd_bus_error *ret_error)
{
    const char *title, *body;
    sd_bus *bus = sd_bus_message_get_bus(m);
    int r;
    (void)userdata; (void)ret_error;

    r = sd_bus_message_read(m, "ss", &title, &body);
    if (r < 0) return r;

    unsigned id = ++counter;
    printf("[서버] 알림 #%u 수신: [%s] %s\n", id, title, body);
    fflush(stdout);

    r = sd_bus_emit_signal(bus, OBJ_PATH, IFACE, "Notified", "uss", id, title, body);
    if (r < 0) fprintf(stderr, "시그널 전송 실패: %s\n", strerror(-r));

    return sd_bus_reply_method_return(m, "u", id);
}

static int prop_count(sd_bus *bus, const char *path, const char *interface,
                      const char *property, sd_bus_message *reply,
                      void *userdata, sd_bus_error *ret_error)
{
    (void)bus; (void)path; (void)interface; (void)property; (void)userdata; (void)ret_error;
    return sd_bus_message_append(reply, "u", counter);
}

static const sd_bus_vtable vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Notify", "ss", "u", method_notify, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Notified", "uss", 0),
    SD_BUS_PROPERTY("Count", "u", prop_count, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_VTABLE_END
};

int main(void)
{
    sd_bus_slot *slot = NULL;
    sd_bus *bus = NULL;
    int r;

    r = sd_bus_open_user(&bus);
    if (r < 0) { fprintf(stderr, "세션 버스 연결 실패: %s\n", strerror(-r)); return 1; }

    r = sd_bus_add_object_vtable(bus, &slot, OBJ_PATH, IFACE, vtable, NULL);
    if (r < 0) { fprintf(stderr, "오브젝트 등록 실패: %s\n", strerror(-r)); goto out; }

    r = sd_bus_request_name(bus, BUS_NAME, 0);
    if (r < 0) { fprintf(stderr, "이름 요청 실패: %s\n", strerror(-r)); goto out; }

    printf("[서버] %s 준비 완료 (종료: Ctrl-C)\n", BUS_NAME);
    fflush(stdout);

    for (;;) {
        r = sd_bus_process(bus, NULL);
        if (r < 0) { fprintf(stderr, "처리 실패: %s\n", strerror(-r)); break; }
        if (r > 0) continue;               /* 더 처리할 메시지가 있음 */
        r = sd_bus_wait(bus, (uint64_t)-1);
        if (r < 0) break;
    }
out:
    sd_bus_slot_unref(slot);
    sd_bus_unref(bus);
    return r < 0 ? 1 : 0;
}
