#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    const char *port = (argc > 1) ? argv[1] : "./ttyV0";

    printf("=== [Level 1: 기본 문자열 송신 예제] ===\n");
    printf("포트: %s (115200 bps, 8N1 Raw)\n", port);

    int fd = serial_open(port, B115200);
    if (fd < 0) {
        fprintf(stderr, "포트 오픈 실패: %s\n", port);
        return 1;
    }

    for (int i = 1; i <= 5; ++i) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Hello Embedded Serial! [Count: %d]\n", i);

        int written = serial_send_raw(fd, (const uint8_t *)msg, strlen(msg));
        printf("[송신 %d] %s (전송 바이트: %d)\n", i, msg, written);

        sleep(1);
    }

    serial_close(fd);
    printf("송신 완료.\n");
    return 0;
}
