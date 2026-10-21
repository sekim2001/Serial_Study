#include "serial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

/**
 * @brief 시리얼 포트를 열고 임베디드 통신에 적합한 Raw 8N1 모드로 설정
 * @param port_name 포트 장치 경로 (예: "/dev/ttyS0", "./ttyV0")
 * @param baudrate 통신 속도 (예: B115200, B9600)
 * @return 성공 시 파일 디스크립터(fd), 실패 시 -1
 */
int serial_open(const char *port_name, speed_t baudrate)
{
    /*
     * O_RDWR   : 읽기 및 쓰기 모드로 오픈
     * O_NOCTTY : 이 포트가 프로세스의 제어 터미널(Controlling TTY)이 되지 않도록 설정
     *            (포트에서 들어오는 Ctrl+C 등의 신호가 이 프로세스를 강제 종료하지 않음)
     * O_NONBLOCK: 오픈 시 하드웨어 캐리어 감지(DCD) 신호를 무한정 기다리지 않도록 비차단 오픈
     */
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("[ERROR] 시리얼 포트 열기 실패");
        return -1;
    }

    /* O_NONBLOCK 플래그를 해제하여 일반 블로킹/I-O 모드로 전환 (이후 poll()로 타임아웃 제어) */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        perror("[ERROR] tcgetattr 실패");
        close(fd);
        return -1;
    }

    /* 1. Baudrate(보레이트) 설정: 송신/수신 동일한 속도 지정 */
    cfsetospeed(&tty, baudrate);
    cfsetispeed(&tty, baudrate);

    /* 2. c_cflag (Control Modes - 하드웨어 제어 플래그) */
    tty.c_cflag &= ~PARENB;         /* Parity 비트 없음 (No Parity) */
    tty.c_cflag &= ~CSTOPB;         /* Stop 비트 1개 (1 Stop bit) */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;             /* Data 비트 8개 (8 Data bits) -> 8N1 완성 */
    tty.c_cflag &= ~CRTSCTS;        /* RTS/CTS 하드웨어 흐름 제어 끄기 */
    tty.c_cflag |= (CREAD | CLOCAL);/* 수신 활성화(CREAD) 및 모뎀 제어선 무시(CLOCAL) */

    /* 3. c_lflag (Local Modes - 터미널 속성 플래그: Raw 모드 핵심) */
    tty.c_lflag &= ~ICANON;         /* Canonical 모드 해제: 개행문자('\n') 없이 바이트 단위 즉시 수신 */
    tty.c_lflag &= ~ECHO;           /* 입력 문자 자동 에코(echo) 비활성화 */
    tty.c_lflag &= ~ECHOE;          /* 백스페이스 에코 비활성화 */
    tty.c_lflag &= ~ISIG;           /* INTR, QUIT 등 터미널 특수 제어 신호 무시 */

    /* 4. c_iflag (Input Modes - 입력 데이터 가공 방지) */
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* 소프트웨어 흐름 제어(XON/XOFF) 끄기 (0x11, 0x13 바이트 필터링 방지) */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); /* 개행 문자 자동 변환 해제 */

    /* 5. c_oflag (Output Modes - 출력 데이터 가공 방지) */
    tty.c_oflag &= ~(OPOST | ONLCR);/* 출력 후처리 및 '\n' -> '\r\n' 변환 해제 (Raw 데이터 보존) */

    /* 6. VMIN / VTIME 설정 (비정규 모드 읽기 파라미터)
     * VMIN  = 0 : 최소 수신 바이트 수 (0으로 두고 poll()과 결합하여 유연하게 제어)
     * VTIME = 0 : 0.1초 단위 타이머 (0이면 즉시 반환, poll() 타임아웃 사용)
     */
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 0;

    /* 시리얼 버퍼 잔여 데이터 플러시 후 즉시 설정 적용 */
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        perror("[ERROR] tcsetattr 실패");
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * @brief 시리얼 포트 닫기
 */
void serial_close(int fd)
{
    if (fd >= 0) {
        tcdrain(fd); /* 송신 버퍼의 모든 데이터가 전송 완료될 때까지 대기 */
        close(fd);
    }
}

/**
 * @brief 로우(Raw) 바이트 배열 송신
 */
int serial_send_raw(int fd, const uint8_t *buf, size_t len)
{
    size_t total_written = 0;
    while (total_written < len) {
        ssize_t written = write(fd, buf + total_written, len - total_written);
        if (written < 0) {
            if (errno == EINTR) continue;
            perror("[ERROR] serial write 실패");
            return -1;
        }
        total_written += written;
    }
    return (int)total_written;
}

/**
 * @brief 로우(Raw) 바이트 수신 (타임아웃 적용)
 * @param timeout_ms 밀리초 단위 대기 시간 (-1이면 무한 대기)
 */
int serial_receive_raw(int fd, uint8_t *buf, size_t max_len, int timeout_ms)
{
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret < 0) {
        if (errno == EINTR) {
            return 0; /* 시그널 인터럽트 발생 */
        }
        perror("[ERROR] poll 실패");
        return -1;
    } else if (ret == 0) {
        /* 타임아웃 발생 */
        return 0;
    }

    if (pfd.revents & POLLIN) {
        ssize_t bytes_read = read(fd, buf, max_len);
        if (bytes_read < 0) {
            perror("[ERROR] serial read 실패");
            return -1;
        }
        return (int)bytes_read;
    }

    return 0;
}

/**
 * @brief 간단한 8비트 XOR 체크섬 계산
 */
uint8_t calculate_checksum(const uint8_t *data, size_t len)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; ++i) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief 구조화된 임베디드 패킷 송신
 *  [STX][SEQ][CMD][LEN][PAYLOAD][CHECKSUM][ETX]
 */
int serial_send_packet(int fd, uint8_t seq, uint8_t cmd, const uint8_t *data, uint8_t len)
{
    if (len > MAX_PAYLOAD_LEN) {
        fprintf(stderr, "[ERROR] 페이로드 길이가 최대 허용치(%d)를 초과했습니다.\n", MAX_PAYLOAD_LEN);
        return -1;
    }

    uint8_t buffer[sizeof(SerialPacket)];
    size_t idx = 0;

    buffer[idx++] = SERIAL_STX;
    buffer[idx++] = seq;
    buffer[idx++] = cmd;
    buffer[idx++] = len;

    if (len > 0 && data != NULL) {
        memcpy(&buffer[idx], data, len);
        idx += len;
    }

    /* 체크섬 계산 대상: SEQ, CMD, LEN, PAYLOAD */
    uint8_t chk = calculate_checksum(&buffer[1], 3 + len);
    buffer[idx++] = chk;
    buffer[idx++] = SERIAL_ETX;

    return serial_send_raw(fd, buffer, idx);
}

/**
 * @brief 상태 머신(Finite State Machine) 기반 임베디드 패킷 수신
 * 스트림에서 노이즈나 분절된 바이트가 들어와도 STX를 감지하고 완전한 프레임을 복원
 */
int serial_receive_packet(int fd, uint8_t *seq, uint8_t *cmd, uint8_t *data, uint8_t *len, int timeout_ms)
{
    enum {
        STATE_WAIT_STX,
        STATE_GET_SEQ,
        STATE_GET_CMD,
        STATE_GET_LEN,
        STATE_GET_PAYLOAD,
        STATE_GET_CHECKSUM,
        STATE_GET_ETX
    } state = STATE_WAIT_STX;

    uint8_t rx_seq = 0;
    uint8_t rx_cmd = 0;
    uint8_t rx_len = 0;
    uint8_t rx_payload[MAX_PAYLOAD_LEN] = {0};
    uint8_t rx_checksum = 0;
    uint8_t payload_idx = 0;

    int remaining_ms = timeout_ms;

    while (1) {
        uint8_t byte;
        int res = serial_receive_raw(fd, &byte, 1, (timeout_ms < 0) ? -1 : remaining_ms);
        if (res <= 0) {
            return res; /* 0: 타임아웃, -1: 에러 */
        }

        switch (state) {
            case STATE_WAIT_STX:
                if (byte == SERIAL_STX) {
                    state = STATE_GET_SEQ;
                }
                break;

            case STATE_GET_SEQ:
                rx_seq = byte;
                state = STATE_GET_CMD;
                break;

            case STATE_GET_CMD:
                rx_cmd = byte;
                state = STATE_GET_LEN;
                break;

            case STATE_GET_LEN:
                rx_len = byte;
                if (rx_len > MAX_PAYLOAD_LEN) {
                    /* 비정상 길이: 프레임 오염으로 판단하고 STX 대기로 복귀 */
                    state = STATE_WAIT_STX;
                } else if (rx_len == 0) {
                    state = STATE_GET_CHECKSUM;
                } else {
                    payload_idx = 0;
                    state = STATE_GET_PAYLOAD;
                }
                break;

            case STATE_GET_PAYLOAD:
                rx_payload[payload_idx++] = byte;
                if (payload_idx >= rx_len) {
                    state = STATE_GET_CHECKSUM;
                }
                break;

            case STATE_GET_CHECKSUM:
                rx_checksum = byte;
                state = STATE_GET_ETX;
                break;

            case STATE_GET_ETX:
                if (byte == SERIAL_ETX) {
                    /* 체크섬 검증 */
                    uint8_t calc_buf[3 + MAX_PAYLOAD_LEN];
                    calc_buf[0] = rx_seq;
                    calc_buf[1] = rx_cmd;
                    calc_buf[2] = rx_len;
                    if (rx_len > 0) {
                        memcpy(&calc_buf[3], rx_payload, rx_len);
                    }
                    uint8_t expected_chk = calculate_checksum(calc_buf, 3 + rx_len);

                    if (expected_chk == rx_checksum) {
                        /* 무결성 검증 성공: 호출자에게 데이터 반환 */
                        if (seq) *seq = rx_seq;
                        if (cmd) *cmd = rx_cmd;
                        if (len) *len = rx_len;
                        if (data && rx_len > 0) {
                            memcpy(data, rx_payload, rx_len);
                        }
                        return 1; /* 패킷 정상 수신 */
                    } else {
                        fprintf(stderr, "[WARN] 체크섬 불일치! (수신: 0x%02X != 계산: 0x%02X)\n",
                                rx_checksum, expected_chk);
                        state = STATE_WAIT_STX;
                    }
                } else {
                    fprintf(stderr, "[WARN] 유효하지 않은 ETX (0x%02X != 0x03)\n", byte);
                    state = STATE_WAIT_STX;
                }
                break;
        }
    }
}
