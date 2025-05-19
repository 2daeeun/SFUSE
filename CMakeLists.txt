cmake_minimum_required(VERSION 4.0.2)

project(SFUSE)
set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# FUSE3 탐색
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

# 타겟명 설정
set(TARGET_NAME sfuse)

# 소스 자동 수집
file(GLOB SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/*.c")

# 실행파일 정의
add_executable(${TARGET_NAME} ${SOURCE_FILES})

# FUSE 헤더 포함
target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include
                                                  ${FUSE3_INCLUDE_DIRS})

# 링커 및 컴파일 플래그
target_link_libraries(${TARGET_NAME} PRIVATE ${FUSE3_LIBRARIES})
target_compile_options(${TARGET_NAME} PRIVATE ${FUSE3_CFLAGS_OTHER} -Wall
                                              -Wextra -Wpedantic)

# FUSE API 버전 명시
target_compile_definitions(${TARGET_NAME} PRIVATE FUSE_USE_VERSION=31)
