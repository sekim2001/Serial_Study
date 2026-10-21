#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

static volatile int running = 1;

void handle_signal(int sig)
{
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[])
{
    const char *port = (argc > 1) ? argv[1] : "./ttyV1";

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    printf("===================================================\n");
    printf("   [수신 장치 (Main Controller)] 시리얼 수신 대기\n");
    printf("   연결 포트: %s (115200 bps, 8N1)\n", port);
    printf("===================================================\n");

    int fd = serial_open(port, B115200);
    if (fd < 0) {
        fprintf(stderr, "포트 '%s' 열기 실패. 가상 포트(socat)가 실행 중인지 확인하세요.\n", port);
        return 1;
    }

    printf("-> 포트 연결 성공! 데이터 수신 대기 중... (Ctrl+C를 누르면 종료)\n\n");

    uint8_t seq = 0;
    uint8_t cmd = 0;
    uint8_t payload[MAX_PAYLOAD_LEN] = {0};
    uint8_t len = 0;

    while (running) {
        /* 타임아웃 1000ms로 패킷 수신 대기 */
        int res = serial_receive_packet(fd, &seq, &cmd, payload, &len, 1000);
        if (res < 0) {
            fprintf(stderr, "[ERROR] 패킷 수신 중 치명적 오류 발생\n");
            break;
        } else if (res == 0) {
            /* 타임아웃: 아직 데이터가 들어오지 않음 */
            continue;
        }

        /* 패킷 정상 수신 처리 */
        printf("===================================================\n");
        printf("[수신 패킷 감지] SEQ: %u | CMD: 0x%02X | LEN: %u 바이트\n", seq, cmd, len);

        uint8_t reply_code = SERIAL_ACK;

        switch (cmd) {
            case CMD_PING:
                printf("-> [CMD_PING] 연결 확인 요청을 수신했습니다.\n");
                printf("<- [ACK 회신] 통신 상태 정상 응답을 보냅니다.\n");
                serial_send_raw(fd, &reply_code, 1);
                break;

            case CMD_SENSOR_DATA:
                if (len == sizeof(SensorPayload)) {
                    SensorPayload *sensor = (SensorPayload *)payload;
                    printf("-> [CMD_SENSOR_DATA] 센서 원격 측정 데이터 분석 결과:\n");
                    printf("   ┌─ 온도:        %.1f °C\n", sensor->temperature / 10.0);
                    printf("   ├─ 습도:        %.1f %%\n", sensor->humidity / 10.0);
                    printf("   └─ 장치 업타임: %u 초\n", sensor->uptime_sec);
                    
                    /* 송신 노드에 수신 완료(ACK) 응답 */
                    serial_send_raw(fd, &reply_code, 1);
                    printf("<- [ACK 회신] 패킷 처리 완료 응답 전송.\n");
                } else {
                    fprintf(stderr, "[WARN] 센서 페이로드 크기 불일치 (기대: %zu, 수신: %u)\n",
                            sizeof(SensorPayload), len);
                    reply_code = SERIAL_NAK;
                    serial_send_raw(fd, &reply_code, 1);
                }
                break;

            default:
                printf("[WARN] 알 수 없는 명령어 수신: 0x%02X\n", cmd);
                reply_code = SERIAL_NAK;
                serial_send_raw(fd, &reply_code, 1);
                break;
        }
        printf("\n");
    }

    printf("\n종료 신호 감지. 포트를 닫습니다...\n");
    serial_close(fd);
    printf("수신 프로그램 종료 완료.\n");

    return 0;
}
