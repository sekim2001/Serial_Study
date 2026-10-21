#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include <stddef.h>
#include <termios.h>

/* 프로토콜 특수 제어 바이트 정의 (ASCII 제어 문자) */
#define SERIAL_STX  0x02  /* Start of Text (프레임 시작) */
#define SERIAL_ETX  0x03  /* End of Text   (프레임 종료) */
#define SERIAL_ACK  0x06  /* Acknowledge   (정상 수신 응답) */
#define SERIAL_NAK  0x15  /* Negative Ack  (오류 응답) */

/* 명령(Command) 코드 정의 */
#define CMD_PING         0x01  /* 연결 확인 */
#define CMD_SENSOR_DATA  0x02  /* 센서 측정값 전송 */
#define CMD_LED_CONTROL  0x03  /* 액추에이터 제어 */

#define MAX_PAYLOAD_LEN  64

/*
 * [임베디드 시리얼 패킷 프레임 구조]
 *  +-----+-----+-----+-----+-----------------+----------+-----+
 *  | STX | SEQ | CMD | LEN | PAYLOAD (LEN B) | CHECKSUM | ETX |
 *  | 1B  | 1B  | 1B  | 1B  | 0 ~ 64 B        | 1B       | 1B  |
 *  +-----+-----+-----+-----+-----------------+----------+-----+
 *
 *  - STX/ETX : 패킷 경계 구분 (Packet Framing)
 *  - SEQ     : 시퀀스 번호 (패킷 유실 및 중복 확인)
 *  - CMD     : 명령어 또는 데이터 타입
 *  - LEN     : 페이로드 길이
 *  - CHECKSUM: 데이터 무결성 검증 (XOR Checksum)
 */

#pragma pack(push, 1)
typedef struct {
    uint8_t stx;
    uint8_t seq;
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[MAX_PAYLOAD_LEN];
    uint8_t checksum;
    uint8_t etx;
} SerialPacket;

/* 센서 데이터 페이로드 구조체 예시 */
typedef struct {
    int16_t temperature;  /* 온도 (단위: 0.1도, 예: 255 -> 25.5도) */
    uint16_t humidity;    /* 습도 (단위: 0.1%,  예: 602 -> 60.2%) */
    uint32_t uptime_sec;  /* 시스템 가동 시간 (초) */
} SensorPayload;
#pragma pack(pop)

/* 함수 선언 */
int serial_open(const char *port_name, speed_t baudrate);
void serial_close(int fd);

int serial_send_raw(int fd, const uint8_t *buf, size_t len);
int serial_receive_raw(int fd, uint8_t *buf, size_t max_len, int timeout_ms);

int serial_send_packet(int fd, uint8_t seq, uint8_t cmd, const uint8_t *data, uint8_t len);
int serial_receive_packet(int fd, uint8_t *seq, uint8_t *cmd, uint8_t *data, uint8_t *len, int timeout_ms);

uint8_t calculate_checksum(const uint8_t *data, size_t len);

#endif /* SERIAL_H */
