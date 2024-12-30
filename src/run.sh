#!/bin/bash

# compile_commands.json 생성 함수
generate_compile_commands() {

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
}

# FUSE 마운트 함수
mount_fuse() {

  # 빌드 디렉토리에서 make 실행
  echo "빌드 디렉토리에서 make 실행 중..."
  make -C build

  # sfuse_filesystem 디렉토리 생성
  echo "sfuse_filesystem 디렉토리 생성 중..."
  mkdir -p sfuse_filesystem

  # 빌드된 sfuse 파일을 사용하여 마운트 실행
  echo "FUSE 마운트 중..."
  ./build/sfuse sfuse_filesystem

  # 마운트 확인
  echo -e "\n---------- 결과 확인하기 ----------\n"
  ls -la sfuse_filesystem
  cat sfuse_filesystem/hello.txt
}

# 옵션 출력
echo -e "┌───────────────옵션───────────────┐"
echo -e "│ 1) compile_commands.json 생성     │"
echo -e "│ 2) 컴파일 & FUSE 마운트           │"
echo -e "│ 3) FUSE 언마운트                  │"
echo -e "│ q) 종료                           │"
echo -e "└───────────────────────────────────┘"

# 사용자 입력 받기
read -p "선택: " choice

case $choice in
1)
  # compile_commands.json 생성 함수 실행
  generate_compile_commands
  ;;
2)
  # compile_commands.json 파일 확인
  if [ ! -f "compile_commands.json" ]; then
    echo "compile_commands.json 파일이 없습니다. 생성합니다..."
    generate_compile_commands
  else
    echo "compile_commands.json 파일이 이미 존재합니다."
  fi

  # FUSE 마운트 함수 실행
  mount_fuse
  ;;
3)
  # FUSE 언마운트
  umount sfuse_filesystem
  rmdir sfuse_filesystem
  ;;
q)
  echo "종료합니다."
  exit 0
  ;;
*)
  echo "잘못된 입력입니다. 스크립트를 다시 실행해주세요."
  exit 1
  ;;
esac
