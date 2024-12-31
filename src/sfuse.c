/*
 * File: sfuse.c
 * FUSE version used: fusermount3 3.16.2
 * Description: A simple file system implemented using FUSE on Linux
 * Version: 0.0.1
 */

#include "fuse_opt.h"
#define FUSE_USE_VERSION 31
#define FILE_MAX_SIZE 1024 /* 가상 파일 최대 크기 */

#include <errno.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/*
 * file_name - 가상 파일의 이름
 * file_content - 가상 파일의 내용
 *
 * FUSE 파일 시스템에서 "/hello.txt"라는 이름의 파일을 생성하며,
 * 해당 파일은 "Hello, World!"라는 내용을 가지고 있습니다.
 */
static const char *file_name = "/hello.txt";
static char *file_content = "Hello, World!";
static size_t file_len;

/*
 * sfuse_getattr
 * : 파일 및 디렉토리의 속성을 반환하는 함수
 *
 * @path: 요청된 파일 또는 디렉토리의 경로
 * @stbuf: 속성 정보를 저장할 stat 구조체
 * @fi: FUSE 파일 정보 구조체 (사용하지 않음)
 *
 * 요청된 경로가 루트 디렉토리("/")인 경우, 디렉토리 속성을 설정합니다.
 * 요청된 경로가 "hello.txt"인 경우, 해당 가상 파일의 속성을 설정합니다.
 * 만약 요청된 경로가 위의 두 경우에 해당하지 않으면 -ENOENT를 반환하여
 * 해당 파일이나 디렉토리가 없음을 알립니다.
 *
 * st_mode: 파일 또는 디렉토리의 유형과 권한을 설정
 * st_nlink: 파일 또는 디렉토리의 하드 링크 수를 설정
 * st_size: 파일 크기를 바이트 단위로 설정
 *
 * 반환 값:
 *   0: 속성 설정 성공
 *  -ENOENT: 요청된 파일이나 디렉토리가 없음
 */
static int sfuse_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
  printf("sfuse_getattr called...");
  (void)fi; /* 사용하지 않음 */

  memset(stbuf, 0, sizeof(struct stat));
  if (strcmp(path, "/") == 0) {
    /* 루트 디렉토리 속성 설정 */
    stbuf->st_mode = S_IFDIR | 0755; /* 디렉토리, 읽기/실행 권한 */
    stbuf->st_nlink = 2;             /* 링크 개수: "."와 ".." */
    return 0;
  } else if (strcmp(path, file_name) == 0) {
    /* "hello.txt" 파일 속성 설정 */
    stbuf->st_mode = S_IFREG | 0444; /* 일반 파일, 읽기 전용 권한 */
    stbuf->st_nlink = 1;             /* 단일 파일 링크 */
    stbuf->st_size = file_len;       /* 파일 크기 */
    return 0;
  } else {
    return -ENOENT; /* 파일 또는 디렉토리가 없음 */
  }

  return 0;
}

/*
 * sfuse_readdir
 * : 디렉토리의 내용을 반환하는 함수
 *
 * @path: 디렉토리 경로
 * @buf: 디렉토리 내용을 저장할 버퍼
 * @filler: 디렉토리 항목을 추가하는 콜백 함수
 * @offset: 디렉토리 읽기 시작 위치 (사용하지 않음)
 * @fi: FUSE 파일 정보 구조체 (사용하지 않음)
 * @flags: 디렉토리 읽기 플래그 (사용하지 않음)
 *
 * 루트 디렉토리("/")의 내용을 반환하며, 다음 항목을 포함합니다:
 *   - 현재 디렉토리(".")
 *   - 부모 디렉토리("..")
 *   - 가상 파일 "hello.txt"
 *
 * @path가 루트 디렉토리가 아닌 경우 -ENOENT를 반환하여
 * 해당 디렉토리가 없음을 알립니다.
 *
 * 반환 값:
 *   0: 성공
 *  -ENOENT: 디렉토리가 없음
 */
static int sfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
  printf("sfuse_readdir called...");
  (void)offset; /* 사용하지 않음 */
  (void)fi;     /* 사용하지 않음 */
  (void)flags;  /* 사용하지 않음 */

  if (strcmp(path, "/") != 0) {
    return -ENOENT; /* 루트 디렉토리가 아니면 오류 반환 */
  }

  /* 디렉토리 항목 추가 */
  filler(buf, ".", NULL, 0, 0);           /* 현재 디렉토리 */
  filler(buf, "..", NULL, 0, 0);          /* 부모 디렉토리 */
  filler(buf, file_name + 1, NULL, 0, 0); /* "hello.txt" 파일 */

  return 0;
}

/*
 * sfuse_open
 * : 파일 열기를 요청하는 함수

 * @path: 열고자 하는 파일 경로
 * @fi: FUSE 파일 정보 구조체 (사용하지 않음)
 *
 * 루트 디렉토리에 정의된 "hello.txt" 파일만 열 수 있도록 제한.
 * 다른 파일 경로가 요청되면 -ENOENT를 반환하여 해당 파일이 없음을 알림.
 *
 * 반환 값:
 *   0: 성공
 *  -ENOENT: 파일이 없음
 */
static int sfuse_open(const char *path, struct fuse_file_info *fi) {
  printf("sfuse_open called...");
  (void)fi; /* 사용하지 않음 */

  if (strcmp(path, file_name) != 0) {
    return -ENOENT; /* 파일이 없음 */
  }

  return 0;
}

/*
 * sfuse_read
 * : 파일 읽기를 요청 처리하는 함수
 *
 * @path: 읽을 파일 경로
 * @buf: 데이터를 저장할 버퍼
 * @size: 요청된 데이터 크기
 * @offset: 읽기 시작 위치
 * @fi: FUSE 파일 정보 구조체 (사용하지 않음)
 *
 * "hello.txt" 파일의 내용을 요청된 위치(offset)에서 지정된 크기(size)만큼
 * 데이터를 읽어 buf에 복사합니다.
 * - 파일 끝을 초과한 offset 요청 시 0을 반환합니다.
 * - 읽기 범위가 파일 크기를 초과하면 남은 데이터만 반환합니다.
 *
 * 반환 값:
 *   - size: 복사한 바이트 수 (성공 시)
 *   - -ENOENT: 파일이 없을 경우
 */
static int sfuse_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
  printf("sfuse_read called...");
  (void)fi; /* 사용하지 않음 */

  if (strcmp(path, file_name) != 0) {
    return -ENOENT; /* 파일이 없음 */
  }

  if (offset >= file_len) {
    return 0; /* 파일 끝을 초과한 경우 */
  }

  if (offset + size > file_len) {
    size = file_len - offset; /* 읽기 크기 조정 (파일 끝까지만 읽음) */
  }

  memcpy(buf, file_content + offset, size);

  return size; /* 읽은 바이트 수 반환 */
}
/*
 * sfuse_write
 */

static int sfuse_write(const char *path, const char *buf, size_t size,
                       off_t offset, struct fuse_file_info *fi) {
  printf("sfuse_write called...");
  (void)fi;

  if (strcmp(path, file_name) != 0) {
    return -ENOENT; /* 파일이 없음 */
  }

  /* 오프셋이 파일 크기를 초과하면 실패 */
  if (offset > FILE_MAX_SIZE) {
    return -EFBIG; /* 파일 크기 초과 */
  }

  /* 쓰기 크기 제한 (file_content의 최대 크기 초과 방지) */
  if (offset + size > FILE_MAX_SIZE) {
    size = FILE_MAX_SIZE - offset; /* 남은 크기만큼 조정 */
  }

  /* 데이터 복사 */
  memcpy(file_content + offset, buf, size);

  /* 파일 크기 업데이트 */
  if (offset + size > file_len) {
    file_len = offset + size;
  }

  return size; /* 성공적으로 복사된 바이트 수 반환 */
}

/*
 * op (fuse_operations)
 * : FUSE 파일 시스템 동작을 정의하는 구조체
 *
 * FUSE 파일 시스템이 처리해야 하는 동작을 사용자 정의 함수로 매핑합니다.
 * 이 구조체는 FUSE 메인 루프에서 사용되며, 파일 시스템 동작을 제어합니다.
 *
 * 각 동작은 다음과 같이 구현됩니다:
 *
 *   - getattr: 파일 또는 디렉토리의 속성을 반환합니다.
 *              루트 디렉토리("/")와 가상 파일("hello.txt")에 대해
 *              속성을 설정하며, 존재하지 않는 파일 요청 시 -ENOENT를
 *              반환합니다.
 *
 *   - readdir: 디렉토리의 내용을 반환합니다.
 *              루트 디렉토리("/")의 경우, ".", "..", "hello.txt"를 포함한
 *              목록을 반환합니다. 다른 디렉토리 요청 시 -ENOENT를 반환합니다.
 *
 *   - open: 파일 열기 요청을 처리합니다.
 *           "hello.txt" 파일만 열 수 있으며, 다른 파일 요청 시 -ENOENT를
 *            반환합니다.
 *
 *   - read: 파일 읽기 요청을 처리합니다.
 *           요청된 위치(offset)부터 크기(size)만큼 데이터를 읽어
 *           버퍼에 복사하며, 파일 끝을 초과한 요청은 0을 반환합니다.
 */
static const struct fuse_operations op = {
    .getattr = sfuse_getattr, /* 파일 및 디렉토리 속성 반환 */
    .readdir = sfuse_readdir, /* 디렉토리 내용 반환 */
    .open = sfuse_open,       /* 파일 열기 요청 처리 */
    .read = sfuse_read,       /* 파일 읽기 요청 처리 */
    .write = sfuse_write,     /* 파일 쓰기 요청 처리*/
};

/*
 * fuse_main
 * : FUSE 메인 루프를 시작하는 함수
 *
 * (argc와 argv의 대한 설명은 생략)
 * @op: FUSE 동작 정의 구조체
 * @private_data: 사용자 정의 데이터를 전달 (현재 NULL로 설정)
 *
 * 주요 역할:
 *   - 명령줄 인수를 파싱하여 FUSE 설정 초기화.
 *   - 사용자 정의 동작(op)을 등록하여 파일 시스템 요청 처리.
 *   - 실행 중 발생하는 파일 시스템 요청을 적절한 핸들러로 전달.
 */
int main(int argc, char *argv[]) {
  file_len = strlen(file_content); // 런타임 초기화

  int ret;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

  ret = fuse_main(argc, argv, &op, NULL);
  fuse_opt_free_args(&args);

  return ret;
}
