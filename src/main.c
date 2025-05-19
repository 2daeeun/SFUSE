// File: src/main.c

#include "fs.h" /**< fs_initialize, fs_teardown */
#include <errno.h>
#include <fuse3/fuse.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** @brief 전역: 포맷 강제 플래그 */
bool g_force_format = false;

/** @brief FUSE 연산 테이블 반환 함수 */
struct fuse_operations *sfuse_get_operations(void);

/**
 * @brief SFUSE 엔트리 포인트
 *
 * 커맨드라인 인자를 파싱하여 파일 시스템을 초기화하고,
 * FUSE 메인 루프를 실행한 후 정리한다.
 *
 * @param argc 인자 개수
 * @param argv 인자 문자열 배열
 * @return FUSE 메인 루프 반환값 또는 EXIT_FAILURE
 */
int main(int argc, char *argv[]) {
  /* -F 옵션 처리 (강제 포맷) */
  bool force = false;
  int idx = 1;
  while (idx < argc && strcmp(argv[idx], "-F") == 0) {
    force = true;
    idx++;
  }
  g_force_format = force;

  /* 인수 체크: 이미지 파일, 마운트 포인트 필요 */
  if (argc - idx < 2) {
    fprintf(stderr, "Usage: %s [-F] <image> <mountpoint> [FUSE options]\n",
            argv[0]);
    return EXIT_FAILURE;
  }
  const char *image_path = argv[idx];     /**< 디스크 이미지 경로 */
  const char *mountpoint = argv[idx + 1]; /**< 마운트 포인트 */
  int remaining = argc - (idx + 2);       /**< FUSE 옵션 개수 */

  /* 파일 시스템 초기화 */
  int err = 0;
  struct sfuse_fs *fs = fs_initialize(image_path, &err);
  if (!fs) {
    if (err == EINVAL) {
      fprintf(stderr,
              "SFUSE: 이미지가 VSFS 형식이 아닙니다. "
              "포맷하려면 -F 옵션을 사용하세요. (err=%d)\n",
              err);
    } else {
      fprintf(stderr, "SFUSE: 파일시스템 초기화 실패 (err=%d)\n", err);
    }
    return EXIT_FAILURE;
  }

  /* FUSE args 준비 */
  int fuse_argc = 2 + remaining;
  char **fuse_argv = malloc(sizeof(char *) * fuse_argc);
  if (!fuse_argv) {
    perror("malloc");
    fs_teardown(fs);
    return EXIT_FAILURE;
  }
  int f = 0;
  fuse_argv[f++] = strdup("sfuse");    /**< 프로그램 이름 */
  fuse_argv[f++] = strdup(mountpoint); /**< 마운트 포인트 */
  for (int i = 0; i < remaining; ++i) {
    fuse_argv[f++] = strdup(argv[idx + 2 + i]); /**< 추가 FUSE 옵션 */
  }
  struct fuse_args fuse_args = FUSE_ARGS_INIT(fuse_argc, fuse_argv);

  /* FUSE 메인 루프 실행 */
  int ret =
      fuse_main(fuse_args.argc, fuse_args.argv, sfuse_get_operations(), fs);

  /* 정리 */
  fs_teardown(fs);
  for (int i = 0; i < fuse_argc; ++i)
    free(fuse_argv[i]);
  free(fuse_argv);
  fuse_opt_free_args(&fuse_args);
  return ret;
}
