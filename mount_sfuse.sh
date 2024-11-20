#!/bin/bash

# Step 1: 실행파일 생성에 필요한 compile_commands 생성
./generate_compile_commands.sh

# Step 2: 빌드 디렉토리에서 make 실행
make -C build

# Step 3: sfuse_filesystem 디렉토리 생성
mkdir -p sfuse_filesystem

# Step 4: 빌드된 sfuse 파일을 사용하여 마운트 실행
./build/sfuse sfuse_filesystem

# Step 5: 마운트 확인하기
echo -e "\n---------- 결과 확인하기 ----------\n"

ls -la sfuse_filesystem
cat sfuse_filesystem/hello.txt
