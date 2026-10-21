#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

static volatile int running = 1;

void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

static void print_hex(const char *label, const uint8_t *data, size_t len)
{
    printf("%s [%zu bytes]: ", label, len);
    for (size_t i = 0; i < len; ++i) {
        printf("%02X ", data[i]);
    }
    printf("\n");
}

int main(int argc, char *argv[])
{
    const char *port = (argc > 1) ? argv[1] : "./ttyV0";

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("===================================================\n");
    printf("   [송신 장치 (Sensor Node)] 시리얼 전송 시작\n");
    printf("   연결 포트: %s (115200 bps, 8N1)\n", port);
    printf("===================================================\n");

    int fd = serial_open(port, B115200);
    if (fd < 0) {
        fprintf(stderr, "포트 '%s' 열기 실패. 가상 포트(socat)가 실행 중인지 확인하세요.\n", port);
        return 1;
    }

    printf("-> 포트 연결 성공! (Ctrl+C를 누르면 종료)\n\n");

    uint8_t seq = 0;
    uint32_t uptime = 0;

    /* 1. 먼저 PING 패킷 전송 테스트 */
    printf("[1] PING 패킷 전송 (연결 점검)\n");
    serial_send_packet(fd, seq++, CMD_PING, NULL, 0);

    /* 수신측의 ACK 대기 (타임아웃 1000ms) */
    uint8_t ack_byte = 0;
    int rx_len = serial_receive_raw(fd, &ack_byte, 1, 1000);
    if (rx_len > 0 && ack_byte == SERIAL_ACK) {
        printf("<- 수신기로부터 [ACK] 수신 완료! 정상 통신 확인됨.\n\n");
    } else {
        printf("<- ACK 응답 대기 시간 초과 (수신 프로그램이 아직 시작되지 않았을 수 있습니다)\n\n");
    }

    /* 2. 주기적인 센서 데이터 전송 루프 */
    srand(time(NULL));
    int16_t base_temp = 240; /* 24.0도 */
    uint16_t base_hum = 550; /* 55.0% */

    while (running) {
        /* 가상 센서 데이터 생성 */
        SensorPayload sensor;
        sensor.temperature = base_temp + (rand() % 30 - 15); /* 22.5도 ~ 25.5도 */
        sensor.humidity    = base_hum  + (rand() % 40 - 20); /* 53.0% ~ 57.0% */
        sensor.uptime_sec  = uptime++;

        printf("---------------------------------------------------\n");
        printf("[송신 #%u] 센서 측정값:\n", seq);
        printf("   - 온도: %.1f °C\n", sensor.temperature / 10.0);
        printf("   - 습도: %.1f %%\n", sensor.humidity / 10.0);
        printf("   - 가동 시간: %u 초\n", sensor.uptime_sec);

        /* 패킷 프레임 전송 */
        print_hex("-> [TX Raw Frame]", (const uint8_t *)&sensor, sizeof(sensor));
        int sent_bytes = serial_send_packet(fd, seq, CMD_SENSOR_DATA,
                                            (uint8_t *)&sensor, sizeof(sensor));
        if (sent_bytes > 0) {
            printf("-> 패킷 전송 완료 (총 %d 바이트)\n", sent_bytes);
        }

        /* 수신 측 ACK 대기 */
        rx_len = serial_receive_raw(fd, &ack_byte, 1, 500);
        if (rx_len > 0 && ack_byte == SERIAL_ACK) {
            printf("<- 수신 장치 응답: [ACK 0x06] (수신 완료)\n");
        } else {
            printf("<- ACK 응답 없음 (Timeout)\n");
        }

        seq++;
        sleep(1); /* 1초 주기로 전송 */
    }

    printf("\n종료 신호 감지. 포트를 닫습니다...\n");
    serial_close(fd);
    printf("송신 프로그램 종료 완료.\n");

    return 0;
}
