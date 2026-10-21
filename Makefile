CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -Iinclude
SRC_DIR = src
INC_DIR = include
BIN_DIR = bin

TARGETS = $(BIN_DIR)/simple_send $(BIN_DIR)/simple_recv $(BIN_DIR)/sender $(BIN_DIR)/receiver

.PHONY: all clean ports clean-ports test-simple test-packet

all: $(BIN_DIR) $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# 공통 시리얼 라이브러리 오브젝트
$(BIN_DIR)/serial.o: $(SRC_DIR)/serial.c $(INC_DIR)/serial.h
	$(CC) $(CFLAGS) -c $< -o $@

# Level 1 예제 바이너리
$(BIN_DIR)/simple_send: $(SRC_DIR)/simple_send.c $(BIN_DIR)/serial.o
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/simple_recv: $(SRC_DIR)/simple_recv.c $(BIN_DIR)/serial.o
	$(CC) $(CFLAGS) $^ -o $@

# Level 2 패킷 프레임 예제 바이너리
$(BIN_DIR)/sender: $(SRC_DIR)/sender.c $(BIN_DIR)/serial.o
	$(CC) $(CFLAGS) $^ -o $@

$(BIN_DIR)/receiver: $(SRC_DIR)/receiver.c $(BIN_DIR)/serial.o
	$(CC) $(CFLAGS) $^ -o $@

# 가상 시리얼 포트 생성 타겟
ports:
	@./setup_virtual_ports.sh

# 가상 시리얼 포트 및 socat 종료
clean-ports:
	@pkill -f "socat.*link=.*ttyV" 2>/dev/null || true
	@rm -f ttyV0 ttyV1
	@echo "가상 시리얼 포트 정리 완료."

clean: clean-ports
	rm -rf $(BIN_DIR)
