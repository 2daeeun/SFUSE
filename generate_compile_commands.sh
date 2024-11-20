#!/bin/bash

# 스크립트 실행 중 오류 발생 시 중단
set -e

# 빌드 디렉토리 이름
BUILD_DIR="build"

# 빌드 디렉토리 생성
if [ ! -d "$BUILD_DIR" ]; then
  echo "build 디렉토리를 생성합니다."
  mkdir "$BUILD_DIR"
else
  echo "build 디렉토리가 이미 존재합니다."
fi

# CMake 실행 및 compile_commands.json 생성
echo "CMake를 실행하여 compile_commands.json을 생성합니다..."
echo -e "\n"
cd "$BUILD_DIR"
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

# compile_commands.json 확인 및 프로젝트 루트로 복사
echo -e "\n"
if [ -f "compile_commands.json" ]; then
  echo "compile_commands.json파일을 성공적으로 생성되었습니다."
  cd ..
  cp "$BUILD_DIR/compile_commands.json" .
  echo "compile_commands.json파일이 복사되었습니다."
else
  echo "오류: compile_commands.json 생성에 실패했습니다."
  exit 1
fi

echo -e "\n"
echo "작업이 완료되었습니다!"
