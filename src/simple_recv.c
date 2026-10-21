#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    const char *port = (argc > 1) ? argv[1] : "./ttyV1";

    printf("=== [Level 1: 기본 문자열 수신 예제] ===\n");
    printf("포트: %s (115200 bps, 8N1 Raw)\n", port);
    printf("데이터 수신 대기 중... (5회 수신 시 자동 종료)\n\n");

    int fd = serial_open(port, B115200);
    if (fd < 0) {
        fprintf(stderr, "포트 오픈 실패: %s\n", port);
        return 1;
    }

    int received_count = 0;
    while (received_count < 5) {
        char buf[256] = {0};
        /* 타임아웃 2000ms */
        int bytes = serial_receive_raw(fd, (uint8_t *)buf, sizeof(buf) - 1, 2000);

        if (bytes > 0) {
            buf[bytes] = '\0';
            received_count++;
            printf("[수신 #%d (%d bytes)]: %s", received_count, bytes, buf);
        } else if (bytes == 0) {
            printf("... 대기 중 (Timeout 2s) ...\n");
        } else {
            fprintf(stderr, "수신 에러\n");
            break;
        }
    }

    serial_close(fd);
    printf("\n수신 완료.\n");
    return 0;
}
