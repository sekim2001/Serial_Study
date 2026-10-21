#!/usr/bin/env bash
# Level 2 패킷 프레이밍 및 센서 데이터 / ACK 통신 데모 스크립트

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT0="$DIR/ttyV0"
PORT1="$DIR/ttyV1"

# 기존 리소스 정리
pkill -f "socat.*link=$PORT0" 2>/dev/null
rm -f "$PORT0" "$PORT1"

cleanup() {
    echo ""
    echo "[정리 중...] 가상 포트 및 프로세스를 종료합니다."
    pkill -P $$ 2>/dev/null || true
    pkill -f "socat.*link=$PORT0" 2>/dev/null || true
    rm -f "$PORT0" "$PORT1"
}
trap cleanup EXIT INT TERM

echo "=========================================================="
echo " [Step 1] 가상 시리얼 포트 쌍 생성 (socat)"
echo "=========================================================="
socat -d -d pty,raw,echo=0,link="$PORT0" pty,raw,echo=0,link="$PORT1" 2>/dev/null &
SOCAT_PID=$!

for i in {1..30}; do
    if [ -e "$PORT0" ] && [ -e "$PORT1" ]; then
        break
    fi
    sleep 0.1
done

if [ ! -e "$PORT0" ] || [ ! -e "$PORT1" ]; then
    echo "[오류] 가상 포트 생성 실패"
    exit 1
fi
echo "✓ 포트 생성 완료: $PORT0 <---> $PORT1"
echo ""

echo "=========================================================="
echo " [Step 2] 수신 장치 (Main Controller: receiver) 백그라운드 시작"
echo "=========================================================="
"$DIR/bin/receiver" "$PORT1" &
RECV_PID=$!
sleep 0.5

echo ""
echo "=========================================================="
echo " [Step 3] 송신 장치 (Sensor Node: sender) 실행"
echo " (약 4초간 센서 데이터 패킷 전송 후 종료됩니다)"
echo "=========================================================="

# 송신기를 백그라운드로 4초 실행 후 SIGINT 전달
"$DIR/bin/sender" "$PORT0" &
SENDER_PID=$!

sleep 4.5
kill -INT $SENDER_PID 2>/dev/null || true
wait $SENDER_PID 2>/dev/null || true

kill -INT $RECV_PID 2>/dev/null || true
wait $RECV_PID 2>/dev/null || true

echo ""
echo "=== Level 2 패킷 통신 데모 성공 완료! ==="
