/**
 * @file src/main.c
 * @brief SFUSE FUSE 파일 시스템 메인 엔트리 포인트
 *
 * SFUSE는 블록 디바이스를 VSFS로 포맷하고 마운트합니다.
 */

#include "fs.h"
#include <errno.h>
#include <fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief SFUSE FUSE 연산 테이블 반환
 */
struct fuse_operations *sfuse_get_operations(void);

/**
 * @brief 프로그램 진입점
 *
 * @param argc 인자 개수
 * @param argv 인자 리스트
 * @return FUSE 메인 루프 반환값
 */
int main(int argc, char *argv[]) {
  if (argc < 3) {
    fprintf(stderr, "Usage: %s <device> <mountpoint> [FUSE options]\n",
            argv[0]);
    return EXIT_FAILURE;
  }
  const char *device_path = argv[1];
  const char *mountpoint = argv[2];

  int err = 0;
  struct sfuse_fs *fs = fs_initialize(device_path, &err);
  if (!fs) {
    fprintf(stderr, "SFUSE: 파일시스템 초기화 실패 (err=%d)\n", err);
    return EXIT_FAILURE;
  }

  int remaining = argc - 3;
  int fuse_argc = 2 + remaining;
  char **fuse_argv = malloc(sizeof(char *) * fuse_argc);
  fuse_argv[0] = argv[0];
  fuse_argv[1] = (char *)mountpoint;
  for (int i = 0; i < remaining; i++) {
    fuse_argv[2 + i] = argv[3 + i];
  }
  struct fuse_args fuse_args = FUSE_ARGS_INIT(fuse_argc, fuse_argv);

  int ret =
      fuse_main(fuse_args.argc, fuse_args.argv, sfuse_get_operations(), fs);

  /* 정리 작업 */
  fs_teardown(fs);
  free(fuse_argv);
  fuse_opt_free_args(&fuse_args);
  return ret;
}
