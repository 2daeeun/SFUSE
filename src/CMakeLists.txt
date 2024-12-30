cmake_minimum_required(VERSION 3.10)

# 프로젝트 이름과 C 표준 설정
project(SFUSE)
set(CMAKE_C_STANDARD 99)

# FUSE 라이브러리 경로 설정
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

# 타겟 이름 변수 설정
set(TARGET_NAME sfuse)

# 소스 파일 추가
add_executable(${TARGET_NAME} sfuse.c)

# FUSE 라이브러리 링크
target_include_directories(${TARGET_NAME} PRIVATE ${FUSE3_INCLUDE_DIRS})
target_link_libraries(${TARGET_NAME} PRIVATE ${FUSE3_LIBRARIES})
target_compile_options(${TARGET_NAME} PRIVATE ${FUSE3_CFLAGS_OTHER})
