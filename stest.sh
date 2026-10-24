#!/bin/bash

# 1. 스크립트가 위치한 compiler 디렉토리로 이동
cd "$(dirname "$0")"

# 2. 기준 테스트 디렉토리 설정
TEST_DIR="./test"

# 3. 인자(파일명) 확인 (예: astnull 또는 test)
if [ -z "$1" ]; then
    echo "사용법: $0 [파일이름]"
    echo "예시: $0 astnull"
    exit 1
fi

FILE_NAME=$1

# 4. 파일 경로 자동 정의
MRS_FILE="${TEST_DIR}/mrs/${FILE_NAME}.mrs"
TOKEN_FILE="${TEST_DIR}/tokens/${FILE_NAME}.txt"
LOG_FILE="${TEST_DIR}/logs/${FILE_NAME}.log"
JSON_FILE="${TEST_DIR}/jsons/${FILE_NAME}.json"

g++ ./lexer/lexer.c++ -o lexer.o
g++ ./parser/RDP.c++ -o parser.o

# 5. 실행 절차
echo "=== [1/2] Lexer 실행 시작 ==="
if [ -f "$MRS_FILE" ]; then
    ./lexer.o "$MRS_FILE" "$TOKEN_FILE"
    echo "Lexer 완료 -> $TOKEN_FILE 생성됨"
else
    echo "오류: $MRS_FILE 파일이 존재하지 않습니다."
    exit 1
fi

echo ""
echo "=== [2/2] Parser 실행 시작 ==="
if [ -f "$TOKEN_FILE" ]; then
    ./parser.o "$TOKEN_FILE" "$LOG_FILE" "$JSON_FILE"
    echo "Parser 완료 -> $LOG_FILE, $JSON_FILE 생성됨"
else
    echo "오류: Parser를 실행할 $TOKEN_FILE 파일이 없습니다."
    exit 1
fi

echo ""
echo "모든 컴파일 테스트가 완료되었습니다."