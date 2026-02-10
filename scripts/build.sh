#!/bin/bash
set -e

echo "=================================================="
echo "SECS UDP Receiver (C++) - 빌드"
echo "=================================================="

# 빌드 디렉토리 생성
mkdir -p build
cd build

# CMake 설정
echo "CMake 설정 중..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_COMPILER=g++

# 빌드
echo "빌드 중..."
make -j$(nproc)

echo "=================================================="
echo "빌드 완료: ./build/secs-receiver"
echo "=================================================="
