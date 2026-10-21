#!/usr/bin/env bash

# 가상 시리얼 포트 쌍(Virtual Serial Port Pair) 생성 스크립트
# socat 도구를 이용해 물리적 RX/TX 핀 없이 가상 터미널(PTY)을 연결합니다.

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PORT0="$DIR/ttyV0"
PORT1="$DIR/ttyV1"

# 기존 심볼릭 링크 및 기존 socat 프로세스 정리
rm -f "$PORT0" "$PORT1"
pkill -f "socat.*link=$PORT0" 2>/dev/null

echo "=========================================================="
echo " [가상 시리얼 케이블 (PTY 쌍) 시작]"
echo "  - 송신/수신 포트 0: $PORT0"
echo "  - 송신/수신 포트 1: $PORT1"
echo "=========================================================="
echo "socat을 실행합니다. 종료하려면 Ctrl+C를 누르세요."
echo ""

exec socat -d -d pty,raw,echo=0,link="$PORT0" pty,raw,echo=0,link="$PORT1"
