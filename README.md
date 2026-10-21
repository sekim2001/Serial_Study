## 시리얼 통신

+ 시리얼 통신은 `/dev/ttyS0`와 같은 문자 디바기읏 노츠를 파일처럼 열고 읽고 쓰는 방식으로 동작
+ **프로그래밍 흐름** : 포트 개방 (Open) → 통신 환경 설정 (termios) → 데이터 송수신 (Read/Write) → 포트 닫기 (Close)

### 1. 포트개방 (`Open`)

```C
  // open (const char *__path, int __oflag, ...)
  int fd = open(port_name, O_RDWR | O_NOCTTY | O_SYNC);
```

+ **동작**: `port_name`(예: /dev/ttyV0) 경로의 장치 파일을 열고 파일 디스트립터(FD)를 반환
+ `O_PDWR`: 읽기 및 쓰기 모드로 포트 개방
+ `O_NOCTTY`: 해당 시리얼 포트가 프로그램의 "Controlling Terminal"이 되지 않도록 설정. 미설정 시 포트로 들어오는 키도브 인터럽트 신호(Ctrl+C 등)가 프로그램을 종료시킬 수 있음
+ `O_SYNC`: 데이터 쓰기(write) 작업이 하드웨어 또는 커널 버퍼에 완전히 기록될 때까지 대기(동기화)하도록 설정
+ `O_NONBLOCK`: 오픈 시 하드웨어 캐리어 감지(DCD) 신호를 기다리지 않도록 비차단 오픈


### 2. 시리얼 통신 환경 설정 (`tcgetattr`, `termios`, `tcsetattr`)

+ `tcgetattr(fd, &tty)`
  + 시리얼 포트(fd)의 기존 터미널 설정값을 읽어와 `struct termios tty` 구조체에 저장한다.
  + 기존 설정을 덮어쓰기 전 현재 상태를 백업하고 수정하기 위해 호출한다.

+ `termios` 구조체
속도와 데이터 규격 (Baud rate, Data bits, Parity 등)을 설정하기 위해 `termios` 구조체를 이용하여 설정

```c
  #include <termios.h>

  struct termios {
      tcflag_t c_iflag;      /* Input Modes   : 입력 데이터 가공/필터링 제어 */
      tcflag_t c_oflag;      /* Output Modes  : 출력 데이터 가공/변환 제어 */
      tcflag_t c_cflag;      /* Control Modes : 하드웨어 물리선(속도, 비트, 패리티) 제어 */
      tcflag_t c_lflag;      /* Local Modes   : 터미널 동작(에코, 정규모드, 시그널) 제어 */
      cc_t     c_line;       /* Line Discipline (보통 0으로 둠) */
      cc_t     c_cc[NCCS];   /* Control Characters : 특수 제어 문자 및 VMIN/VTIME */
      speed_t  c_ispeed;     /* Input Baudrate (cfsetispeed 함수로 설정 권장) */
      speed_t  c_ospeed;     /* Output Baudrate (cfsetospeed 함수로 설정 권장) */
  };

  /* c_cflag (Control Modes - 하드웨어 제어 플래그) */
  tty.c_cflag &= ~PARENB;         /* Parity 비트 없음 (No Parity) */
  tty.c_cflag &= ~CSTOPB;         /* Stop 비트 1개 (1 Stop bit) */
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;             /* Data 비트 8개 (8 Data bits) -> 8N1 완성 */
  tty.c_cflag &= ~CRTSCTS;        /* RTS/CTS 하드웨어 흐름 제어 끄기 */
  tty.c_cflag |= (CREAD | CLOCAL);/* 수신 활성화(CREAD) 및 모뎀 제어선 무시(CLOCAL) */

  /* c_lflag (Local Modes - 터미널 속성 플래그: Raw 모드 핵심) */
  tty.c_lflag &= ~ICANON;         /* Canonical 모드 해제: 개행문자('\n') 없이 바이트 단위 즉시 수신 */
  tty.c_lflag &= ~ECHO;           /* 입력 문자 자동 에코(echo) 비활성화 */
  tty.c_lflag &= ~ECHOE;          /* 백스페이스 에코 비활성화 */
  tty.c_lflag &= ~ISIG;           /* INTR, QUIT 등 터미널 특수 제어 신호 무시 */

  /* c_iflag (Input Modes - 입력 데이터 가공 방지) */
  tty.c_iflag &= ~(IXON | IXOFF | IXANY); /* 소프트웨어 흐름 제어(XON/XOFF) 끄기 (0x11, 0x13 바이트 필터링 방지) */
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL); /* 개행 문자 자동 변환 해제 */

  /* c_oflag (Output Modes - 출력 데이터 가공 방지) */
  tty.c_oflag &= ~(OPOST | ONLCR);/* 출력 후처리 및 '\n' -> '\r\n' 변환 해제 (Raw 데이터 보존) */

  /* VMIN / VTIME 설정 (비정규 모드 읽기 파라미터)
    * VMIN  = 0 : 최소 수신 바이트 수 (0으로 두고 poll()과 결합하여 유연하게 제어)
    * VTIME = 0 : 0.1초 단위 타이머 (0이면 즉시 반환, poll() 타임아웃 사용)
    */
  tty.c_cc[VMIN] = 0;
  tty.c_cc[VTIME] = 0;
  ```

+ 통신 속도(Baud Rate) 설정 (`cfsetispeed`, `cfsetospeed`)
  ```c
    cfsetospeed(&tty, baudrate);
    cfsetispeed(&tty, baudrate);
  ```
  + 입력(ispeed) 및 출력(ospeed) 통신 속도를 baudrate(115,200 bps)로 설정
  + 물리적 시리얼 포트가 아닌 가상 터미널(PTY) 환경에서는 논리적 의미만 갖음

+ `tcsetattr(fd, TCSANOW, &tty)`
  + 구조체에 구성한 설정값들을 포트에 실제로 적용
  + `TCSANOW` 인자는 변경 사항을 즉시 적용을 의미

---
## 표준 시리얼 통신 설정 (Raw 8N1)

### 8N1 하드웨어 전송 규격

  + Start(1비트) + Data(8비트) + Stop(1비트) =  `총 10비트` 규격으로 전송 라인을 통해 송신 되도록 설정
  + **Baudrate의 bps 단위** 임으로 프레임 오버헤드를 포함한 **전송비트 / Baudrate** 통산 전송 시간 연산 가능

| 약어 | 의미 | 내용 |
| :--: | :--: | :--: |
| 8 | __8 Data Bits__ | **8비트(1바이트)** 의 데이터 길이 | 
| N | __No Parity__ | 오류 검출용 Parity 비트를 사용하지 않음 (None / Even / Odd) |
| 1 | __1 Stop Bit__ | 데이터 전송 끝을 알리는 Stop Bit를 1비트 길이 유지 |

### Raw 모드

+ 터미널의 모든 참견(버퍼링, 에코, 특수문자 변환, 시그널 생성)을 비활성
+ 하드웨어 라인에 들어오는 바이트를 Raw 그래도 프로그램의 메모리로 1:1 직렬 설정 

### 코드 구현

```c
    /* ================= [1. 8N1 하드웨어 설정] ================= */
    tty.c_cflag &= ~PARENB;         // [N] Parity 비트 없음
    tty.c_cflag &= ~CSTOPB;         // [1] Stop 비트 1개
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;             // [8] Data 비트 8개
    tty.c_cflag &= ~CRTSCTS;        // 하드웨어 흐름 제어 끔
    
    /* ================= [2. Raw 모드 소프트웨어 설정] ================= */
    // c_lflag: 터미널 제어 플래그
    tty.c_lflag &= ~ICANON;         // Canonical 모드 해제 (엔터키 없이 즉시 수신)
    tty.c_lflag &= ~ECHO;           // 입력 문자 자동 화면 출력(Echo) 끔
    tty.c_lflag &= ~ISIG;           // 0x03(Ctrl+C) 등 시그널 처리 끔 (프로세스 보호)
    
    // c_iflag: 입력 가공 플래그
    tty.c_iflag &= ~(IXON | IXOFF); // 0x11, 0x13 소프트웨어 흐름제어 끔
    tty.c_iflag &= ~(ICRNL | INLCR);// 줄바꿈 문자('\n', '\r') 자동 변환 끔
    
    // c_oflag: 출력 가공 플래그9*
    tty.c_oflag &= ~(OPOST | ONLCR);// 출력 시 개행 문자 변환 등 후처리 끔
```
---

## socat (socket cat)
두 개의 독립적인 데이터 스트림을 연결하여 양방향(Bidirectional) 데이터 전송을 수행하는 리눅스 명령줄 도구

1. 주소 바인딩 및 FD 할당
  `socat`은 `<address1>`과 `<address2>` 인자를 해석하여 각각에 맞는 시스템 콜(`open()`, `socket()`, `accept()`, `pipe()` 등)을 호출하고 두 개의 파일 디스크립터(FD_1, FD_2)를 획득

2. 이중화 루프(Full-Duplex Event Loop):
  `select()` 또는 `poll()`/`epoll()` 시스템 콜을 통해 두 FD를 동시에 감시

3. 데이터 교차 전달 (Cross-Relay):
    + `FD_1`에 읽을 데이터가 들어오면: `read(FD_1)` -> `write(FD_2)`
    + `FD_2`에 읽을 데이터가 들어오면: `read(FD_2)` -> `write(FD_1)`


### 1. TCP Port Forwarding

socat의 포트 포워딩은 네트워크 계층에서 수신된 TCP 패킷을 읽어 대상 주소 및 포트로 카피한 뒤 양방향 스트림을 유지하는 프로세스이다

#### 실행 명령어

```
  socat TCP-LISTEN:8080,fork,reuseaddr TCP:192.168.1.100:80
```

#### 동작 구조 및 핵심 옵션

1. `TCP-LISTEN:8080` (Listen Address)
    + 로컬 시스템 TCP 8080 포트를 바인딩하고 Listen 상태로 전환

2. `frok` (Multi-client Handling)
    + 클라이언트 연결 요청 `SYN`이 들어오면 `accept()` 시스템 콜 호출 후 `fork()`를 실행하여 자식 프로세스 생성
    + 메인 프로세스는 계속 연결 대기하며, 생성된 자식 프로세스가 해당 클라이언트 세션의 트래픽 중계를 담당한다. 이 옵션이 없으면 단 1회 연결 처리 후 `socat` 종료

3. `reuseaddr` (Socket Option)
    + 소켓 옵션인 `SO_RESEADDR`를 설정. 프로세스가 재시작되거나 종료된 직후 `TIME_WAIT` 상태에 들어간 포트를 즉시 재바인딩

4. `TCP:192.168.1.100:80` (Target Address)
    + 연결이 확립되면 대상 IP(192.168.1.100)와 포트(80)로 새로운 TCP 소켓 연결(connect())을 생성 후 두 소켓 간 파일 디스크립터를 상호 이중화(Duplex) 연결

### 2. 가상 시리얼 포트(PTY) 쌍 생성

PTY(가상터미널, Pesudoterminal) 쌍 생성은 물리적 RS-232/RS-422 케이블(Null Modem Cable) 접속 환경을 커널의 터미널 Subsystem 상에서 가상으로 모사하는 기능. 시리얼 드라이버나 제어 프로그램을 물리 하드웨어 없이 루프백 텍스트할 때 사용.

#### 실명 명령어

```
  socat PTY,raw,echo=0,link=/tmp/ttyV0 PTY,raw,echo=0,link=/tmp/ttyV1
```

#### 동작 구조 및 핵심 요소

1. `PTY` (Pseudoterminal Driver)
    + 커널의 `/dev/ptmx` 장치를 열어 PTY Master/Slave 쌍을 각각 할당
    + 두 개의 PTY 두소를 인자로 넘기면, socat은 한쪽 PTY Slave의 입력(Tx)을 다른쪽 PTY Slave의 출력(Rx)으로 상호 교차 바인딩

2. `raw` (Line Discipline Disable)
    + 리눅스의 tty의 기본 Line Discipline (터미널 규격) 기능을 비활성화
    + 개행 문자 (CR/LF)변환, Ctrl+C (SIGINT) 인터럽트 signal 처이 등을 수행하지 않고, 전달되는 바이트 데이터를 1:1 바이너리 형태로 전달

3. `echo=0` (Local Echo Off)
    + 송신한 데이터가 터미널 로컬 에코에 의해 입력 버퍼로 되돌아오는 현상 방지

4. `link=/tmp/ttyV0` (Symlink Creation)
    + 커널의 의해 동적으로 할당되는 가상 포트명(예:`/dev/pts/3`, `/dev/pts/4`) 대신 애플리케이션이 고정적으로 참조할 수 있는 심볼릭 링크 파일 `/tmp/ttyV0` 및 `/tmp/ttyV1`을 자동으로 생성 (socat 종료 시 해당 링크 파일 삭제)

### 3. 응용 예시

#### Serial Master -> TCP Server 

물리 시리얼 포트 `/dev/ttyUSB0`의 데이터를 읽어 TCP 9000 포트로 노출

```
  socat Open:/dev/ttyUSB0,b115200,raw,echo=0 TCP-LISTEN:9000,reuseaddr,fork
```

#### TCP Client -> Virtual PTY

원격 TCP `9000` 포트에 접속하여 가상 시리얼 포트 `/tmp/ttyV0`에 매핑 

```
  socat PTY,raw,echo=0,link=/tmp/ttyV0 TCP:192.168.1.50:9000
```