#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>

int setup_serial_port(const char* port_name) {
    int fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        printf("Error opening %s: %s\n", port_name, strerror(errno));
        return -1;
    }

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        printf("Error from tcgetattr\n"); return -1;
    }

    // Non-canonical (Raw) 모드 설정: 바이트 단위 즉시 처리
    tty.c_cflag &= ~PARENB;        // No parity bit
    tty.c_cflag &= ~CSTOPB;        // 1 stop bit
    tty.c_cflag &= ~CSIZE;         // Mask data size
    tty.c_cflag |= CS8;            // 8 data bits
    tty.c_cflag &= ~CRTSCTS;       // No hardware flow control
    tty.c_cflag |= CREAD | CLOCAL; // Enable receiver, ignore modem control lines

    tty.c_lflag &= ~ICANON;        // Disable canonical mode (Raw mode)
    tty.c_lflag &= ~ECHO;          // Disable local echo
    tty.c_lflag &= ~ISIG;          // Disable signals (INTR, QUIT, SUSP)

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // Disable software flow control
    tty.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable special input handling

    tty.c_oflag &= ~OPOST;         // Raw output
    tty.c_oflag &= ~ONLCR;         // Prevent newline conversion

    // Timeout 설정 (Non-blocking read)
    tty.c_cc[VTIME] = 10;          // Wait for up to 1s (10 deciseconds)
    tty.c_cc[VMIN] = 0;            // Return immediately with whatever is available

    // Baud rate 설정 (PTY에서는 논리적으로만 적용)
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr\n"); return -1;
    }
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s /dev/pts/X\n", argv[0]); return -1;
    }

    int serial_fd = setup_serial_port(argv[1]);
    if (serial_fd < 0) return -1;

    char *msg = "Hello Virtual UART!\n";
    write(serial_fd, msg, strlen(msg));
    printf("Transmitted: %s", msg);

    char read_buf[256];
    memset(&read_buf, '\0', sizeof(read_buf));
    
    printf("Waiting for response...\n");
    int num_bytes = read(serial_fd, &read_buf, sizeof(read_buf));
    
    if (num_bytes > 0) {
        printf("Received %d bytes: %s\n", num_bytes, read_buf);
    } else {
        printf("No data received or timeout.\n");
    }

    close(serial_fd);
    return 0;
}