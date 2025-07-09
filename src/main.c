/**
 * @file src/main.c
 * @brief SFUSE 파일시스템의 메인 진입점
 *
 * 블록 디바이스를 사용하여 SFUSE 파일시스템을 마운트하는
 * 프로그램의 메인 함수를 구현한다. 파일시스템의 초기화,
 * 디바이스 유효성 검사 및 FUSE 인자 설정 후 실행된다.
 */

#include "fs.h"
#include "ops.h"
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief 프로그램 메인 함수
 *
 * SFUSE 파일시스템을 지정된 블록 디바이스에 마운트하고,
 * FUSE를 통해 사용자의 추가 옵션과 함께 운영한다.
 * 블록 디바이스 유효성을 확인하고 메모리 및 자원을 관리한다.
 *
 * @param argc 명령줄 인자의 개수
 * @param argv 명령줄 인자 배열:
 *        - argv[0]: 프로그램 이름
 *        - argv[1]: 블록 디바이스 경로
 *        - argv[2]: 파일시스템 마운트 지점
 *        - argv[3...n]: FUSE 추가 옵션들
 *
 * @return 프로그램 성공 시 EXIT_SUCCESS 반환, 실패 시 EXIT_FAILURE 반환
 */
int main(int argc, char *argv[]) {
  if (argc >= 2 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))) {
    fprintf(stdout,
            "사용법 : sudo %s <device> <mountpoint> [options]\n"
            "(사용예: sudo ./sfuse /dev/sdx /mnt/partition -f -s -d -o "
            "allow_other,default_permissions\n"
            "\n기본 옵션들:\n"
            "  -f: FUSE 파일시스템을 포그라운드에서 실행한다.\n"
            "  -s: 싱글 스레드 모드로 실행한다(디버깅 용이).\n"
            "  -d: 디버그 모드를 활성화하여 파일시스템의 상세한 동작 정보를 "
            "출력한다.\n"
            "  -o allow_other: 모든 사용자가 파일시스템에 접근 가능하도록 "
            "허용한다.\n"
            "  -o default_permissions: 기본 UNIX 권한 체크를 활성화하여 파일 "
            "접근 권한을 관리한다.\n"
            "\n추가로 사용할 수 있는 FUSE 옵션들:\n"
            "  -h, --help: FUSE 사용법과 옵션 목록을 출력한다.\n"
            "  -V, --version: FUSE 라이브러리의 버전 정보를 출력한다.\n"
            "  -o ro: 읽기 전용으로 파일시스템을 마운트한다.\n"
            "  -o rw: 읽기 및 쓰기 모드로 마운트한다(기본값).\n"
            "  -o fsname=NAME: 파일시스템의 이름을 설정한다.\n"
            "  -o uid=N: 파일의 소유자 UID를 설정한다.\n"
            "  -o gid=N: 파일의 그룹 GID를 설정한다.\n"
            "  -o umask=MASK: 파일 및 디렉토리 생성 시 적용할 권한 마스크를 "
            "설정한다.\n"
            "  -o nonempty: 마운트 지점이 비어있지 않아도 마운트를 허용한다.\n"
            "  -o direct_io: 커널 페이지 캐시를 우회하여 직접 입출력을 "
            "활성화한다.\n"
            "  -o auto_unmount: 프로그램 종료 시 자동으로 마운트를 해제한다.\n",
            argv[0]);
    return EXIT_SUCCESS;
  }

  if (argc < 3) {
    fprintf(stderr, "사용법: sudo %s <device> <mountpoint> [options]\n",
            argv[0]);
    return EXIT_FAILURE;
  }

  /** 마운트할 블록 디바이스의 경로를 저장하는 포인터 */
  const char *dev_path = argv[1];
  /** SFUSE 파일시스템을 마운트할 디렉토리 경로를 저장하는 포인터 */
  const char *mountpoint = argv[2];

  /**
   * 블록 디바이스 파일을 열고 파일 디스크립터를 획득한다.
   *
   * open 함수는 파일이나 디바이스를 열 때 사용하며,
   * O_RDWR는 읽기 및 쓰기 모드,
   * O_SYNC는 데이터를 즉시 디스크에 쓰는 동기화 모드를 설정한다.
   */
  int backing_fd = open(dev_path, O_RDWR | O_SYNC);
  if (backing_fd < 0) {
    perror("디바이스 열기 실패");
    return EXIT_FAILURE;
  }

  /**
   * 열린 디바이스가 올바른 블록 디바이스인지 확인한다.
   *
   * fstat 함수는 파일 디스크립터의 상태를 stat 구조체에 저장하며,
   * S_ISBLK 매크로를 사용하여 블록 디바이스 여부를 검사한다.
   */
  struct stat st;
  if (fstat(backing_fd, &st) < 0 || !S_ISBLK(st.st_mode)) {
    fprintf(stderr, "%s는 블록 디바이스가 아닙니다.\n", dev_path);
    close(backing_fd);
    return EXIT_FAILURE;
  }

  /**
   * SFUSE 파일시스템 관리를 위한 구조체(sfuse_fs)에 메모리를 할당하고
   * 초기화한다.
   * calloc 사용하여 메모리를 0으로 초기화한다.
   */
  struct sfuse_fs *fs = calloc(1, sizeof(*fs));
  if (!fs) {
    perror("메모리 할당 실패");
    close(backing_fd);
    return EXIT_FAILURE;
  }

  /** SFUSE 파일시스템에서 사용할 디바이스 파일 디스크립터를 저장 */
  fs->backing_fd = backing_fd;

  /** FUSE 라이브러리에 전달할 인자 구조체를 초기화한다.
   * FUSE_ARGS_INIT 매크로는 구조체를 안전하게 초기화하는데 사용된다.
   */
  struct fuse_args args = FUSE_ARGS_INIT(0, NULL);

  /** FUSE에 프로그램 이름과 마운트 포인트를 기본 인자로 설정하여 전달한다.
   * argv[0]은 프로그램 이름을 나타내며,
   * argv[2]는 파일시스템 마운트 지점이다.
   */
  fuse_opt_add_arg(&args, argv[0]);
  fuse_opt_add_arg(&args, mountpoint);

  /** 명령줄에서 추가로 입력된 모든 옵션을 FUSE 인자에 추가한다.
   * 이를 통해 사용자는 FUSE의 다양한 기능을 옵션으로 활성화할 수 있다.
   */
  for (int i = 3; i < argc; i++) {
    fuse_opt_add_arg(&args, argv[i]);
  }

  /**
   * 필수 옵션을 추가하여 SFUSE 운영에 필요한 기본 설정을 활성화한다.
   * -o: FUSE에 옵션을 전달할 때 사용하는 플래그이다. 여러 옵션을 쉼표로
   * 구분하여 함께 전달할 수 있다.
   * - allow_other: 다른 사용자가 마운트된 파일시스템에 접근할 수 있도록
   * 허용한다.
   * - default_permissions: 파일시스템의 기본적인 권한 관리를 FUSE가 수행하도록
   * -d: 파일시스템의 동작과 관련된 자세한 디버깅 정보를 출력한다.
   * 한다.
   */
  fuse_opt_add_arg(&args, "-o");
  fuse_opt_add_arg(&args, "allow_other");
  fuse_opt_add_arg(&args, "default_permissions");
  fuse_opt_add_arg(&args, "-d");

  /** FUSE 메인 루프를 실행하여 파일시스템의 실제 마운트 및 운영을 시작한다.
   * sfuse_ops는 파일시스템 작업을 처리하는 함수 포인터를 포함한 구조체이다.
   */
  int ret = fuse_main(args.argc, args.argv, &sfuse_ops, fs);

  /**
   * 사용한 모든 자원을 정리하고 메모리를 해제한 뒤 프로그램을 종료한다.
   * fuse_opt_free_args: FUSE 인자 관련 메모리 해제
   * close: 파일 디스크립터 닫기
   * free: 동적 할당된 메모리 해제
   */
  fuse_opt_free_args(&args);
  close(backing_fd);
  free(fs);

  return ret;
}
