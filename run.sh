#!/bin/bash

# 1) compile_commands.json 생성 함수
GENERATE_COMPILE_COMMANDS() {
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

# 2) compile 함수
COMPILE() {
  # compile_commands.json 파일 확인
  if [ ! -f "compile_commands.json" ]; then
    echo "compile_commands.json 파일이 없습니다. 생성합니다..."
    GENERATE_COMPILE_COMMANDS
  else
    echo "compile_commands.json 파일이 이미 존재합니다."
  fi

  # build 디렉토리 생성
  rm -r build
  mkdir build

  # cmake 실행
  echo -e "\n"
  echo "cmake 실행 중..."
  cmake -DCMAKE_BUILD_TYPE=Debug -S . -B build

  # make 실행
  echo -e "\n"
  echo "빌드 디렉토리에서 make 실행 중..."
  make -C build

  echo -e "\n"
  echo "작업이 완료되었습니다!"
}

# 3) mount 함수
MOUNT() {
  # 디스크 이미지(.img) 파일 생성
  if [[ -f ./build/sfuse.img ]]; then
    echo -e "\n"
    echo "디스크 이미지(.img) 파일이 이미 존재합니다."
  else
    mkdir -p ./build
    truncate -s 200M ./build/sfuse.img
    echo -e "\n"
    echo -e "200MB 크기의 디스크 이미지(.img) 파일 생성이 완료되었습니다!"
    echo -e "(이미지 파일 위치: ./build/sfuse.img) "
  fi

  # 마운트 포인트 생성
  if [[ -d /run/media/leedaeeun/sfuse ]]; then
    echo -e "\n"
    echo -e "마운트 포인트에 디렉토리가 이미 존재합니다"
    echo -e "(마운트 포인트 위치: /tmp/sfuse) "
  else
    mkdir -p /run/media/leedaeeun/sfuse
    echo -e "\n"
    echo -e "마운트 포인트 생성이 완료되었습니다!"
    echo -e "(마운트 포인트 위치: /run/media/leedaeeun/sfuse) "
  fi

  # 마운트 실행
  echo -e "\n"
  echo -e "마운트를 실행합니다."
  echo -e "\n"
  echo -e "마운트 명령어:"
  echo -e "cd build && sudo ./sfuse -F sfuse.img /run/media/leedaeeun/sfuse -f -s -d"
  echo -e "\n"
  sudo ./build/sfuse -F ./build/sfuse.img /run/media/leedaeeun/sfuse -f -s -d

  # 언마운트
  # sudo fusermount3 -u /tmp/sfuse
}

# 옵션 출력
echo -e "┌───────────────옵션───────────────┐"
echo -e "│ 1) compile_commands.json 생성    │"
echo -e "│ 2) 컴파일                        │"
echo -e "│ 3) 파일 기반 블록 디바이스 마운트│"
echo -e "│ q) 종료                          │"
echo -e "└──────────────────────────────────┘"

# 사용자 입력 받기
read -p "선택: " choice

case $choice in
1)
  # compile_commands.json 생성 함수 실행
  GENERATE_COMPILE_COMMANDS
  ;;
2)

  # compile 함수 실행
  COMPILE
  ;;
3)
  # MOUNT 함수 실행
  MOUNT
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
