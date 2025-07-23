// File: src/fs.c

#include "fs.h"
#include "bitmap.h"
#include "block.h"
#include "dir.h"
#include "inode.h"
#include "super.h"
#include <errno.h>
#include <fuse3/fuse.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/** @brief 전역: 포맷 강제 여부 (-F 옵션) */
extern bool g_force_format;

/**
 * @brief 빈 이미지 파일을 VSFS 구조로 포맷
 *
 * 디스크 이미지 파일을 초기화하여 슈퍼블록, 비트맵, 루트 아이노드를 설정하고
 * 기록한다.
 *
 * @param fd      디스크 이미지 파일 디스크립터
 * @param sb      초기화된 슈퍼블록 정보를 저장할 구조체 포인터
 * @return 성공 시 0, 실패 시 -1
 */
static int fs_format_filesystem(int fd, struct sfuse_superblock *sb) {
  struct stat st;
  if (fstat(fd, &st) < 0)
    return -1;
  uint32_t total_all = st.st_size / SFUSE_BLOCK_SIZE;

  /* 비트맵 블록 수 계산 (이미지 크기에 따라 동적 결정) */
  uint32_t bits_per_block = SFUSE_BLOCK_SIZE * 8;
  uint32_t bm_blocks = (total_all + bits_per_block - 1) / bits_per_block;
  uint32_t inodes_per_block = SFUSE_BLOCK_SIZE / sizeof(struct sfuse_inode);
  uint32_t it_blocks =
      (SFUSE_MAX_INODES + inodes_per_block - 1) / inodes_per_block;

  /* 디스크 용량 체크 */
  if (total_all <= 1 + 1 + bm_blocks + it_blocks)
    return -1;

  /* 슈퍼블록 설정 */
  sb->magic = SFUSE_MAGIC;
  sb->total_inodes = SFUSE_MAX_INODES;
  sb->inode_bitmap_start = 1;
  sb->block_bitmap_start = 2;
  sb->inode_table_start = 2 + bm_blocks;
  sb->data_block_start = sb->inode_table_start + it_blocks;
  sb->total_blocks = total_all - sb->data_block_start;
  sb->free_inodes = SFUSE_MAX_INODES;
  sb->free_blocks = sb->total_blocks;

  /* 비트맵 초기화 (0으로 채움) */
  struct sfuse_bitmaps bmaps;
  memset(&bmaps, 0, sizeof(bmaps));
  /* 아이노드 0,1 예약 (루트) */
  bmaps.inode.map[0] |= 0x03;
  sb->free_inodes -= 2;

  /* 루트 inode 초기화 */
  struct sfuse_inode root;
  memset(&root, 0, sizeof(root));
  root.mode = S_IFDIR | 0755;
  root.uid = getuid();
  root.gid = getgid();
  root.size = 0;
  time_t now = time(NULL);
  root.atime = root.mtime = root.ctime = (uint32_t)now;

  /* 디스크에 기록 */
  /* 1) 슈퍼블록 */
  if (pwrite(fd, sb, SFUSE_BLOCK_SIZE, 0) != SFUSE_BLOCK_SIZE)
    return -1;
  /* 2) 아이노드 비트맵 */
  if (pwrite(fd, &bmaps.inode, SFUSE_BLOCK_SIZE,
             sb->inode_bitmap_start * SFUSE_BLOCK_SIZE) < 0)
    return -1;
  /* 3) 블록 비트맵 */
  if (pwrite(fd, &bmaps.block, bm_blocks * SFUSE_BLOCK_SIZE,
             sb->block_bitmap_start * SFUSE_BLOCK_SIZE) < 0)
    return -1;
  /* 4) 루트 아이노드 */
  if (inode_sync(fd, sb, 1, &root) < 0)
    return -1;

  return 0;
}

/**
 * @brief 파일 시스템 초기화
 *
 * 디스크 이미지 파일을 열고 슈퍼블록, 비트맵, 아이노드 테이블을 메모리에
 * 로드한다.
 *
 * @param image_path 디스크 이미지 파일 경로
 * @param error_out  오류 코드를 저장할 포인터 (NULL이 아닌 경우에만 설정됨)
 * @return 초기화된 sfuse_fs 포인터, 실패 시 NULL (error_out에 오류 코드 설정)
 */
struct sfuse_fs *fs_initialize(const char *image_path, int *error_out) {
  struct sfuse_fs *fs = calloc(1, sizeof(struct sfuse_fs));
  if (!fs) {
    *error_out = ENOMEM;
    return NULL;
  }

  /* 디스크 이미지 열기 */
  fs->backing_fd = open(image_path, O_RDWR);
  if (fs->backing_fd < 0) {
    *error_out = errno;
    free(fs);
    return NULL;
  }

  /* 슈퍼블록 로드 */
  int r = sb_load(fs->backing_fd, &fs->sb);
  if (r < 0) {
    if (r == -EINVAL && g_force_format) {
      /* 포맷되지 않았으면 자동 포맷 수행 */
      fprintf(stderr, "SFUSE: 이미지 미포맷 감지, 자동 포맷 수행\n");
      if (fs_format_filesystem(fs->backing_fd, &fs->sb) < 0) {
        *error_out = EIO;
        close(fs->backing_fd);
        free(fs);
        return NULL;
      }
    } else {
      /* 기타 오류 */
      *error_out = (r == -EINVAL ? EINVAL : EIO);
      close(fs->backing_fd);
      free(fs);
      return NULL;
    }
  }

  /* 비트맵 구조체 할당 */
  fs->bmaps = malloc(sizeof(struct sfuse_bitmaps));
  if (!fs->bmaps) {
    *error_out = ENOMEM;
    close(fs->backing_fd);
    free(fs);
    return NULL;
  }

  /* 비트맵 블록 수 계산 및 로드 */
  uint32_t bits_per_block = SFUSE_BLOCK_SIZE * 8;
  uint32_t im_blocks =
      (fs->sb.total_inodes + bits_per_block - 1) / bits_per_block;
  uint32_t bm_blocks =
      (fs->sb.total_blocks + bits_per_block - 1) / bits_per_block;
  if (bitmap_load(fs->backing_fd, fs->sb.inode_bitmap_start, fs->bmaps,
                  im_blocks + bm_blocks) < 0) {
    *error_out = EIO;
    free(fs->bmaps);
    close(fs->backing_fd);
    free(fs);
    return NULL;
  }

  /* 아이노드 테이블 블록 수 계산 및 메모리 할당 */
  uint32_t inode_blocks = (fs->sb.total_inodes + SFUSE_INODES_PER_BLOCK - 1) /
                          SFUSE_INODES_PER_BLOCK;
  fs->inode_table = malloc(inode_blocks * SFUSE_BLOCK_SIZE);
  if (!fs->inode_table) {
    *error_out = ENOMEM;
    free(fs->bmaps);
    close(fs->backing_fd);
    free(fs);
    return NULL;
  }

  /* 아이노드 테이블 읽기 */
  for (uint32_t i = 0; i < inode_blocks; ++i) {
    if (read_block(fs->backing_fd, fs->sb.inode_table_start + i,
                   &fs->inode_table[i]) < 0) {
      *error_out = EIO;
      free(fs->inode_table);
      free(fs->bmaps);
      close(fs->backing_fd);
      free(fs);
      return NULL;
    }
  }

  *error_out = 0;
  return fs;
}

/**
 * @brief 파일 시스템 해제
 *
 * 슈퍼블록과 비트맵을 디스크에 동기화하고, 할당된 메모리 및 파일 디스크립터
 * 정리
 *
 * @param fs 해제할 파일 시스템 컨텍스트 포인터
 */
void fs_teardown(struct sfuse_fs *fs) {
  if (!fs)
    return;

  /* 슈퍼블록 동기화 */
  sb_sync(fs->backing_fd, &fs->sb);

  /* 비트맵 동기화 */
  uint32_t bits_per_block = SFUSE_BLOCK_SIZE * 8;
  uint32_t im_blocks =
      (fs->sb.total_inodes + bits_per_block - 1) / bits_per_block;
  uint32_t bm_blocks =
      (fs->sb.total_blocks + bits_per_block - 1) / bits_per_block;
  bitmap_sync(fs->backing_fd, fs->sb.inode_bitmap_start, fs->bmaps,
              im_blocks + bm_blocks);

  /* 메모리 및 파일 디스크립터 정리 */
  free(fs->inode_table);
  free(fs->bmaps);
  close(fs->backing_fd);
  free(fs);
}

/**
 * @brief 경로 문자열을 inode 번호로 변환
 *
 * 루트("/")는 inode 1로 처리하며, 경로를 '/' 구분자로 분할하여 순차 검색
 *
 * @param fs 파일 시스템 컨텍스트 포인터
 * @param path 변환할 경로 문자열 (null-terminated)
 * @param out_ino 변환된 inode 번호를 저장할 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *out_ino) {
  if (strcmp(path, "/") == 0) {
    *out_ino = 1;
    return 0;
  }
  char *path_copy = strdup(path);
  if (!path_copy)
    return -ENOMEM;
  uint32_t current = 1; // 루트 아이노드부터 시작
  char *saveptr;
  char *token = strtok_r(path_copy, "/", &saveptr);
  while (token) {
    uint32_t next;
    int ret = dir_lookup(fs, current, token, &next);
    if (ret < 0) {
      free(path_copy);
      return ret; // 오류 전파
    }
    current = next;
    token = strtok_r(NULL, "/", &saveptr);
  }
  free(path_copy);
  *out_ino = current;
  return 0;
}

/**
 * @brief 파일 또는 디렉터리 속성 가져오기 (stat)
 *
 * 지정 경로의 inode를 조회하여 stat 구조체에 채움
 *
 * @param fs 파일 시스템 컨텍스트 포인터
 * @param path 대상 경로 문자열
 * @param stbuf 속성 정보를 채울 stat 구조체 포인터
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_getattr(struct sfuse_fs *fs, const char *path, struct stat *stbuf) {
  uint32_t ino;
  int ret = fs_resolve_path(fs, path, &ino);
  if (ret < 0)
    return ret;

  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;

  /* stat 구조체 초기화 및 채우기 */
  memset(stbuf, 0, sizeof(*stbuf));
  stbuf->st_mode = inode.mode;
  stbuf->st_nlink = S_ISDIR(inode.mode) ? 2 : 1;
  stbuf->st_uid = inode.uid;
  stbuf->st_gid = inode.gid;
  stbuf->st_size = inode.size;
  stbuf->st_atime = inode.atime;
  stbuf->st_mtime = inode.mtime;
  stbuf->st_ctime = inode.ctime;
  return 0;
}

/**
 * @brief 디렉터리 목록 가져오기
 *
 * 지정 경로의 디렉터리 엔트리를 FUSE callback에 전달
 *
 * @param fs 파일 시스템 컨텍스트 포인터
 * @param path 디렉터리 경로 문자열
 * @param buf FUSE가 제공하는 버퍼 포인터
 * @param filler FUSE 디렉터리 엔트리 추가 콜백
 * @param offset 읽기 시작 오프셋 (사용되지 않음)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_readdir(struct sfuse_fs *fs, const char *path, void *buf,
               fuse_fill_dir_t filler, off_t offset) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  return dir_list(fs, ino, buf, filler, offset);
}

/**
 * @brief 파일 열기 (존재 여부만 확인)
 *
 * path가 유효한 파일인지 확인
 *
 * @param fs 파일 시스템 컨텍스트 포인터
 * @param path 열고자 하는 파일 경로 문자열
 * @param fi FUSE 파일 정보 구조체 (사용하지 않음)
 * @return 성공 시 0, 실패 시 -ENOENT
 */
int fs_open(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi) {
  (void)fi;
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  return 0;
}

/**
 * @brief 파일 읽기
 *
 * 지정된 경로의 파일 데이터를 읽어 사용자 버퍼에 복사한다.
 *
 * @param fs     파일 시스템 컨텍스트 포인터
 * @param path   읽을 파일 경로 (null-terminated 문자열)
 * @param buf    데이터를 저장할 사용자 버퍼 포인터
 * @param size   요청한 읽기 크기 (바이트 단위)
 * @param offset 파일 내 시작 오프셋 (바이트 단위)
 * @return 읽은 바이트 수 (>=0), 실패 시 음수 오류 코드
 */
int fs_read(struct sfuse_fs *fs, const char *path, char *buf, size_t size,
            off_t offset) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;

  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;

  /* 디렉터리는 읽을 수 없음 */
  if ((inode.mode & S_IFDIR) == S_IFDIR)
    return -EISDIR;

  /* 오프셋이 파일 크기를 넘으면 0 리턴 */
  if ((size_t)offset >= inode.size)
    return 0;

  /* 요청 크기가 파일 끝을 넘으면 조정 */
  if ((size_t)offset + size > inode.size)
    size = inode.size - offset;

  size_t bytes_read = 0;
  uint8_t block_buf[SFUSE_BLOCK_SIZE];

  /* 필요한 블록을 반복해서 읽음 */
  while (bytes_read < size) {
    uint32_t block_index =
        (offset + bytes_read) / SFUSE_BLOCK_SIZE; /**< 논리 블록 인덱스 */
    uint32_t block_offset =
        (offset + bytes_read) % SFUSE_BLOCK_SIZE; /**< 블록 내 오프셋 */
    uint32_t to_read =
        SFUSE_BLOCK_SIZE - block_offset; /**< 이터레이션당 최대 읽기 바이트 */

    if (to_read > size - bytes_read)
      to_read = size - bytes_read;

    uint32_t disk_block = 0;

    /* 직접 블록 참조 */
    if (block_index < 12U) {
      disk_block = inode.direct[block_index];
    }
    /* 단일 간접 블록 참조 */
    else if (block_index < 12U + SFUSE_PTRS_PER_BLOCK) {
      if (!inode.indirect)
        break;
      uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
      if (read_block(fs->backing_fd, inode.indirect, ptrs) < 0)
        return -EIO;
      disk_block = ptrs[block_index - 12U];
    }
    /* 이중 간접 블록 참조 */
    else {
      if (!inode.double_indirect)
        break;
      uint32_t l1[SFUSE_PTRS_PER_BLOCK];
      if (read_block(fs->backing_fd, inode.double_indirect, l1) < 0)
        return -EIO;
      uint32_t dbl_index = block_index - (12U + SFUSE_PTRS_PER_BLOCK);
      uint32_t l1_idx = dbl_index / SFUSE_PTRS_PER_BLOCK;
      uint32_t l2_idx = dbl_index % SFUSE_PTRS_PER_BLOCK;
      if (l1_idx >= SFUSE_PTRS_PER_BLOCK || l1[l1_idx] == 0)
        break;
      uint32_t l2[SFUSE_PTRS_PER_BLOCK];
      if (read_block(fs->backing_fd, l1[l1_idx], l2) < 0)
        return -EIO;
      disk_block = l2[l2_idx];
    }

    /* 할당되지 않은 블록이면 0으로 채움 */
    if (disk_block == 0) {
      memset(buf + bytes_read, 0, to_read);
    } else {
      /* 실제 디스크 읽기 */
      if (read_block(fs->backing_fd, disk_block, block_buf) < 0)
        return -EIO;
      memcpy(buf + bytes_read, block_buf + block_offset, to_read);
    }
    bytes_read += to_read;
  }

  return bytes_read;
}

/**
 * @brief 파일 쓰기
 *
 * 지정된 경로의 파일에 사용자 버퍼의 데이터를 기록한다.
 *
 * @param fs     파일 시스템 컨텍스트 포인터
 * @param path   쓰기 대상 파일 경로 (null-terminated 문자열)
 * @param buf    기록할 데이터가 담긴 버퍼 포인터
 * @param size   기록할 바이트 수
 * @param offset 파일 내 시작 오프셋 (바이트 단위)
 * @return 기록된 바이트 수 (>=0), 실패 시 음수 오류 코드
 */
int fs_write(struct sfuse_fs *fs, const char *path, const char *buf,
             size_t size, off_t offset) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;

  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;

  /*< 디렉터리에는 쓰기 불가 */
  if ((inode.mode & S_IFDIR) == S_IFDIR)
    return -EISDIR;

  size_t bytes_written = 0;
  uint8_t block_buf[SFUSE_BLOCK_SIZE];

  /*< 요청한 크기만큼 반복 처리 */
  while (bytes_written < size) {
    off_t cur_offset = offset + bytes_written; /**< 현재 오프셋 */
    uint32_t block_index =
        cur_offset / SFUSE_BLOCK_SIZE; /**< 논리 블록 인덱스 */
    uint32_t block_offset =
        cur_offset % SFUSE_BLOCK_SIZE; /**< 블록 내 오프셋 */
    size_t to_write =
        SFUSE_BLOCK_SIZE - block_offset; /**< 블록 경계까지 쓸 바이트 */
    if (to_write > size - bytes_written)
      to_write = size - bytes_written;

    uint32_t *target = NULL; /**< 대상 블록 포인터 위치 */
    uint32_t disk_block = 0; /**< 실제 할당된 디스크 블록 번호 */

    /*< 직접 블록 */
    if (block_index < 12U) {
      target = &inode.direct[block_index];
      disk_block = *target; /* direct 블록 포인터로 disk_block 초기화 */
    }
    /*< 단일 간접 블록 */
    else if (block_index < 12U + SFUSE_PTRS_PER_BLOCK) {
      if (inode.indirect == 0) {
        /* 간접 블록이 없으면 새 할당 */
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0)
          return -ENOSPC;
        inode.indirect =
            (uint32_t)(fs->sb.data_block_start +
                       new_block); /* 간접 블록 주소도 절대 번호로 변환 */
        uint32_t tmp[SFUSE_PTRS_PER_BLOCK] = {0};
        write_block(fs->backing_fd, inode.indirect, tmp);
      }
      /* 기존 간접 블록에서 포인터 읽기 */
      uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.indirect, ptrs);

      target = &ptrs[block_index - 12U];
      disk_block = *target;

      /* 새 데이터 블록 할당 */
      if (disk_block == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0)
          return -ENOSPC;
        disk_block = (uint32_t)(fs->sb.data_block_start +
                                new_block); /* 새 블록 인덱스를 데이터 영역
                                               오프셋과 합산 */
        *target = disk_block;
        write_block(fs->backing_fd, inode.indirect, ptrs);
      }
    }
    /*< 이중 간접 블록 */
    else {
      uint32_t dbl_index = block_index - (12U + SFUSE_PTRS_PER_BLOCK);
      uint32_t l1_idx = dbl_index / SFUSE_PTRS_PER_BLOCK; /**< 1차 인덱스 */
      uint32_t l2_idx = dbl_index % SFUSE_PTRS_PER_BLOCK; /**< 2차 인덱스 */

      if (inode.double_indirect == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0)
          return -ENOSPC;
        inode.double_indirect =
            (uint32_t)(fs->sb.data_block_start +
                       new_block); /* 이중 간접 블록도 절대 번호로 변환 */
        uint32_t tmp[SFUSE_PTRS_PER_BLOCK] = {0};
        write_block(fs->backing_fd, inode.double_indirect, tmp);
      }

      uint32_t l1[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.double_indirect, l1);

      if (l1[l1_idx] == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0)
          return -ENOSPC;
        l1[l1_idx] = (uint32_t)(fs->sb.data_block_start +
                                new_block); /* 레벨1 인덱스에도 데이터 영역 시작
                                               오프셋 적용 */
        uint32_t tmp[SFUSE_PTRS_PER_BLOCK] = {0};
        write_block(fs->backing_fd, l1[l1_idx], tmp);
        write_block(fs->backing_fd, inode.double_indirect, l1);
      }

      uint32_t l2[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, l1[l1_idx], l2);

      if (l2[l2_idx] == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0)
          return -ENOSPC;
        l2[l2_idx] = (uint32_t)(fs->sb.data_block_start +
                                new_block); /* 레벨2 인덱스에도 오프셋을 더해
                                               절대 주소로 */
        write_block(fs->backing_fd, l1[l1_idx], l2);
      }

      disk_block = l2[l2_idx];
      target = &l2[l2_idx];
    }

    /*< 아직 할당되지 않았으면 새로 할당 */
    if (target && *target == 0) {
      int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_block < 0)
        return -ENOSPC;
      *target = (uint32_t)(fs->sb.data_block_start +
                           new_block); /* 상대 인덱스에 데이터 영역 시작 블록
                                          번호 더해 절대 블록 번호로 저장 */
      disk_block = *target;
    }

    /*< 기존 블록 읽기 */
    if (read_block(fs->backing_fd, disk_block, block_buf) < 0)
      return -EIO;

    /*< 데이터 쓰기 */
    memcpy(block_buf + block_offset, buf + bytes_written, to_write);
    if (write_block(fs->backing_fd, disk_block, block_buf) < 0)
      return -EIO;

    bytes_written += to_write;
  }

  /*< 파일 크기 갱신 */
  if ((uint32_t)(offset + bytes_written) > inode.size)
    inode.size = offset + bytes_written;
  inode.mtime = time(NULL);  /**< 수정 시간 갱신 */
  inode.ctime = inode.mtime; /**< 변경 시간 갱신 */
  // if (inode_sync(fs->backing_fd, &fs->sb, ino, &inode) < 0)
  //   return -EIO;
  inode_mark_dirty(ino, &inode); // 즉시 디스크 기록 대신 dirty로 마킹

  return bytes_written;
}

/**
 * @brief 새 파일 생성
 *
 * 지정된 경로에 새 파일을 생성하고 상위 디렉터리에 디렉터리 엔트리를 추가한다.
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param path 생성할 파일 경로 (null-terminated 문자열)
 * @param mode 파일 모드 및 권한 (하위 12비트 사용)
 * @param fi   FUSE 파일 정보 구조체 (사용되지 않음)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_create(struct sfuse_fs *fs, const char *path, mode_t mode,
              struct fuse_file_info *fi) {
  (void)fi;

  uint32_t dummy;
  /* 이미 존재하면 실패 */
  if (fs_resolve_path(fs, path, &dummy) == 0) {
    return -EEXIST;
  }

  /* 상위 디렉터리와 새 파일 이름 분리 */
  char *path_copy = strdup(path);
  if (!path_copy)
    return -ENOMEM;
  char *name = strrchr(path_copy, '/');
  if (!name || *(name + 1) == '\0') {
    free(path_copy);
    return -EINVAL;
  }
  *name = '\0';
  char *parent_path = (*path_copy == '\0') ? "/" : path_copy;
  name++;

  /* 상위 디렉터리 inode 찾기 */
  uint32_t parent_ino;
  if (fs_resolve_path(fs, parent_path, &parent_ino) < 0) {
    free(path_copy);
    return -ENOENT;
  }

  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  /* 상위가 디렉터리인지 확인 */
  if ((parent_inode.mode & S_IFDIR) == 0) {
    free(path_copy);
    return -ENOTDIR;
  }

  /* 새 아이노드 할당 */
  int new_ino = alloc_inode(&fs->sb, &fs->bmaps->inode);
  if (new_ino < 0) {
    free(path_copy);
    return new_ino;
  }

  /* 새 아이노드 초기화 */
  struct sfuse_inode new_inode;
  memset(&new_inode, 0, sizeof(new_inode));
  new_inode.mode = (mode & 0xFFF) | S_IFREG; /**< 일반 파일로 설정 */
  new_inode.uid = getuid();
  new_inode.gid = getgid();
  new_inode.size = 0;
  time_t now = time(NULL);
  new_inode.atime = (uint32_t)now;
  new_inode.mtime = (uint32_t)now;
  new_inode.ctime = (uint32_t)now;

  /* 상위 디렉터리에 엔트리 추가 */
  uint8_t dir_block[SFUSE_BLOCK_SIZE];
  bool added = false;
  for (int i = 0; i < 12 && !added; ++i) {
    if (parent_inode.direct[i] == 0) {
      /* 새 디렉터리 블록 할당 및 초기화 */
      int new_dir_block = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_dir_block < 0) {
        free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
        free(path_copy);
        return -ENOSPC;
      }
      parent_inode.direct[i] =
          fs->sb.data_block_start + (uint32_t)new_dir_block;

      memset(dir_block, 0, SFUSE_BLOCK_SIZE);
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      entries[0].inode = new_ino;
      strncpy(entries[0].name, name, SFUSE_NAME_MAX - 1);
      entries[0].name[SFUSE_NAME_MAX - 1] = '\0';

      write_block(fs->backing_fd, parent_inode.direct[i], dir_block);
      parent_inode.size += SFUSE_BLOCK_SIZE;
      added = true;
    } else {
      /* 기존 블록에서 빈 슬롯 검색 */
      if (read_block(fs->backing_fd, parent_inode.direct[i], dir_block) < 0) {
        free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
        free(path_copy);
        return -EIO;
      }
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
        if (entries[j].inode == 0) {
          entries[j].inode = new_ino;
          strncpy(entries[j].name, name, SFUSE_NAME_MAX - 1);
          entries[j].name[SFUSE_NAME_MAX - 1] = '\0';
          write_block(fs->backing_fd, parent_inode.direct[i], dir_block);
          added = true;
          break;
        }
      }
    }
  }

  if (!added) {
    /* 공간 부족 시 롤백 */
    free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
    free(path_copy);
    return -ENOSPC;
  }

  /* 새 아이노드를 디스크에 기록 */
  inode_sync(fs->backing_fd, &fs->sb, new_ino, &new_inode);

  /* 상위 디렉터리 메타데이터 갱신 */
  parent_inode.mtime = (uint32_t)now;
  parent_inode.ctime = (uint32_t)now;
  inode_sync(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);

  free(path_copy);
  return 0;
}

/**
 * @brief 새 디렉터리 생성
 *
 * 지정된 경로에 새 디렉터리를 생성하고 상위 디렉터리에 디렉터리 엔트리를
 * 추가한다.
 *
 * @param fs    파일 시스템 컨텍스트 포인터
 * @param path  생성할 디렉터리 경로 (null-terminated 문자열)
 * @param mode  디렉터리 모드 및 권한 (하위 12비트 사용)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_mkdir(struct sfuse_fs *fs, const char *path, mode_t mode) {
  uint32_t dummy;
  /* 이미 존재하면 실패 */
  if (fs_resolve_path(fs, path, &dummy) == 0) {
    return -EEXIST;
  }

  /* 상위 디렉터리와 새 이름 분리 */
  char *path_copy = strdup(path);
  if (!path_copy)
    return -ENOMEM;
  char *name = strrchr(path_copy, '/');
  if (!name || *(name + 1) == '\0') {
    free(path_copy);
    return -EINVAL;
  }
  *name = '\0';
  char *parent_path = (*path_copy == '\0') ? "/" : path_copy;
  name++;

  /* 상위 디렉터리 inode 조회 */
  uint32_t parent_ino;
  if (fs_resolve_path(fs, parent_path, &parent_ino) < 0) {
    free(path_copy);
    return -ENOENT;
  }

  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  /* 상위가 디렉터리가 아니면 오류 */
  if ((parent_inode.mode & S_IFDIR) == 0) {
    free(path_copy);
    return -ENOTDIR;
  }

  /* 새 아이노드 할당 */
  int new_ino = alloc_inode(&fs->sb, &fs->bmaps->inode);
  if (new_ino < 0) {
    free(path_copy);
    return new_ino;
  }

  /* 새 아이노드 초기화 */
  struct sfuse_inode new_inode;
  memset(&new_inode, 0, sizeof(new_inode));
  new_inode.mode = (mode & 0xFFF) | S_IFDIR; /**< 디렉터리 타입 설정 */
  new_inode.uid = getuid();
  new_inode.gid = getgid();
  new_inode.size = 0;
  time_t now = time(NULL);
  new_inode.atime = (uint32_t)now;
  new_inode.mtime = (uint32_t)now;
  new_inode.ctime = (uint32_t)now;

  /* 상위 디렉터리에 엔트리 추가 (메모리 안전성: 블록 전체 초기화) */
  uint8_t dir_block[SFUSE_BLOCK_SIZE];
  bool added = false;
  for (int i = 0; i < 12 && !added; ++i) {
    if (parent_inode.direct[i] == 0) {
      /* 새 디렉터리 블록 할당 */
      int new_dir_block = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_dir_block < 0) {
        free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
        free(path_copy);
        return -ENOSPC;
      }
      parent_inode.direct[i] =
          fs->sb.data_block_start + (uint32_t)new_dir_block;

      memset(dir_block, 0, SFUSE_BLOCK_SIZE);
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      entries[0].inode = new_ino; /**< 첫 엔트리에 새 inode 설정 */
      strncpy(entries[0].name, name, SFUSE_NAME_MAX - 1);
      entries[0].name[SFUSE_NAME_MAX - 1] = '\0';

      write_block(fs->backing_fd, parent_inode.direct[i], dir_block);
      parent_inode.size += SFUSE_BLOCK_SIZE; /**< 디렉터리 크기 갱신 */
      added = true;
    } else {
      /* 기존 블록에서 빈 슬롯 검색 */
      if (read_block(fs->backing_fd, parent_inode.direct[i], dir_block) < 0) {
        free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
        free(path_copy);
        return -EIO;
      }
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
        if (entries[j].inode == 0) {
          entries[j].inode = new_ino; /**< 빈 슬롯에 inode 설정 */
          strncpy(entries[j].name, name, SFUSE_NAME_MAX - 1);
          entries[j].name[SFUSE_NAME_MAX - 1] = '\0';
          write_block(fs->backing_fd, parent_inode.direct[i], dir_block);
          added = true;
          break;
        }
      }
    }
  }

  /* 공간 부족 시 롤백 */
  if (!added) {
    free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
    free(path_copy);
    return -ENOSPC;
  }

  /* 새 아이노드 디스크에 기록 */
  inode_sync(fs->backing_fd, &fs->sb, new_ino, &new_inode);

  /* 상위 디렉터리 메타데이터 갱신 */
  parent_inode.mtime = (uint32_t)now; /**< 수정 시간 갱신 */
  parent_inode.ctime = (uint32_t)now; /**< 변경 시간 갱신 */
  inode_sync(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);

  free(path_copy);
  return 0;
}

/**
 * @brief 파일 삭제
 *
 * 지정된 경로의 파일을 삭제하고, 관련 데이터 블록 및 아이노드를 해제한다.
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param path 삭제할 파일 경로 (null-terminated 문자열)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_unlink(struct sfuse_fs *fs, const char *path) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }

  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  /* 디렉터리는 삭제 불가 */
  if ((inode.mode & S_IFDIR) == S_IFDIR) {
    return -EISDIR;
  }

  /* 상위 디렉터리 및 파일 이름 분리 */
  char *path_copy = strdup(path);
  if (!path_copy)
    return -ENOMEM;
  char *name = strrchr(path_copy, '/');
  *name = '\0';
  char *parent_path = (*path_copy == '\0') ? "/" : path_copy;
  name++;

  /* 상위 디렉터리 inode 조회 */
  uint32_t parent_ino;
  fs_resolve_path(fs, parent_path, &parent_ino);

  uint8_t dir_block[SFUSE_BLOCK_SIZE];
  uint8_t zero_block[SFUSE_BLOCK_SIZE] = {0}; /* 블록 초기화용 제로 버퍼 */
  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);

  /* 상위 디렉터리에서 디렉터리 엔트리 제거 */
  for (uint32_t i = 0; i < 12; ++i) {
    if (parent_inode.direct[i] == 0)
      continue;
    if (read_block(fs->backing_fd, parent_inode.direct[i], dir_block) < 0)
      continue;
    struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
    for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode == ino &&
          strncmp(entries[j].name, name, SFUSE_NAME_MAX) == 0) {
        /* 삭제 시 디렉터리 엔트리 전체를 0으로 초기화 */
        memset(&entries[j], 0, sizeof(entries[j]));
        write_block(fs->backing_fd, parent_inode.direct[i], dir_block);
        goto found_entry;
      }
    }
  }
found_entry:

  /* 직접 데이터 블록 해제: 내용 제로화 후 상대 인덱스로 비트맵 해제 */
  for (int i = 0; i < 12; ++i) {
    if (inode.direct[i] == 0)
      continue;
    write_block(fs->backing_fd, inode.direct[i],
                zero_block); // 블록 내용 모두 0으로 덮어쓰기
    free_block(&fs->sb, &fs->bmaps->block,
               inode.direct[i] -
                   fs->sb.data_block_start); // 상대 블록 번호로 비트맵 해제
    inode.direct[i] = 0;
  }

  /* 단일 간접 블록 해제: 데이터 블록과 포인터 블록 모두 제로화 후 해제 */
  if (inode.indirect != 0) {
    uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
    read_block(fs->backing_fd, inode.indirect, ptrs);
    for (uint32_t k = 0; k < SFUSE_PTRS_PER_BLOCK; ++k) {
      if (ptrs[k] == 0)
        continue;
      write_block(fs->backing_fd, ptrs[k], zero_block);
      free_block(&fs->sb, &fs->bmaps->block, ptrs[k] - fs->sb.data_block_start);
      ptrs[k] = 0;
    }
    write_block(fs->backing_fd, inode.indirect,
                zero_block); // 포인터 블록도 제로화
    free_block(&fs->sb, &fs->bmaps->block,
               inode.indirect - fs->sb.data_block_start);
    inode.indirect = 0;
  }

  /* 이중 간접 블록 해제: 이중 레벨 순회하며 제로화 후 해제 */
  if (inode.double_indirect != 0) {
    uint32_t l1[SFUSE_PTRS_PER_BLOCK];
    read_block(fs->backing_fd, inode.double_indirect, l1);
    for (uint32_t i1 = 0; i1 < SFUSE_PTRS_PER_BLOCK; ++i1) {
      if (l1[i1] == 0)
        continue;
      uint32_t l2[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, l1[i1], l2);
      for (uint32_t i2 = 0; i2 < SFUSE_PTRS_PER_BLOCK; ++i2) {
        if (l2[i2] == 0)
          continue;
        write_block(fs->backing_fd, l2[i2], zero_block);
        free_block(&fs->sb, &fs->bmaps->block,
                   l2[i2] - fs->sb.data_block_start);
        l2[i2] = 0;
      }
      write_block(fs->backing_fd, l1[i1], zero_block); // 2차 포인터 블록 제로화
      free_block(&fs->sb, &fs->bmaps->block, l1[i1] - fs->sb.data_block_start);
      l1[i1] = 0;
    }
    write_block(fs->backing_fd, inode.double_indirect,
                zero_block); // 1차 포인터 블록 제로화
    free_block(&fs->sb, &fs->bmaps->block,
               inode.double_indirect - fs->sb.data_block_start);
    inode.double_indirect = 0;
  }

  /* 아이노드 비트맵 해제 및 아이노드 초기화 */
  free_inode(&fs->sb, &fs->bmaps->inode, ino);
  struct sfuse_inode empty_inode;
  memset(&empty_inode, 0, sizeof(empty_inode));
  inode_sync(fs->backing_fd, &fs->sb, ino, &empty_inode);

  /* 상위 디렉터리 메타데이터 갱신 */
  time_t now_time = time(NULL);
  parent_inode.mtime = (uint32_t)now_time;
  parent_inode.ctime = (uint32_t)now_time;
  inode_sync(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);

  free(path_copy);
  return 0;
}

/**
 * @brief 비어있는 디렉터리를 삭제
 *
 * 지정된 경로의 디렉터리가 비어 있는지 확인 후,
 * 해당 디렉터리의 inode와 데이터 블록을 해제하고
 * 상위 디렉터리에서 엔트리를 삭제한다.
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param path 삭제할 디렉터리 경로 (null-terminated 문자열)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_rmdir(struct sfuse_fs *fs, const char *path) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;

  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);

  /* 디렉터리인지 확인 */
  if (!(inode.mode & S_IFDIR))
    return -ENOTDIR;

  /* 디렉터리 비어 있는지 확인 */
  uint8_t block[SFUSE_BLOCK_SIZE];
  for (int i = 0; i < 12; ++i) {
    if (inode.direct[i] == 0)
      continue;
    if (read_block(fs->backing_fd, inode.direct[i], block) < 0)
      return -EIO;
    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
    for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode != 0)
        return -ENOTEMPTY;
    }
  }

  /* 상위 디렉터리 및 파일 이름 분리 */
  char *path_copy = strdup(path);
  if (!path_copy)
    return -ENOMEM;
  char *name = strrchr(path_copy, '/');
  *name = '\0';
  char *parent_path = (*path_copy == '\0') ? "/" : path_copy;
  name++;

  /* 상위 디렉터리 inode 조회 */
  uint32_t parent_ino;
  if (fs_resolve_path(fs, parent_path, &parent_ino) < 0) {
    free(path_copy);
    return -ENOENT;
  }

  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);

  /* 상위 디렉터리에서 엔트리 제거 */
  bool removed = false;
  for (uint32_t i = 0; i < 12 && !removed; ++i) {
    if (parent_inode.direct[i] == 0)
      continue;
    read_block(fs->backing_fd, parent_inode.direct[i], block);
    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
    for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode == ino &&
          strncmp(entries[j].name, name, SFUSE_NAME_MAX) == 0) {
        memset(&entries[j], 0, sizeof(entries[j]));
        write_block(fs->backing_fd, parent_inode.direct[i], block);
        removed = true;
        break;
      }
    }
  }

  if (!removed) {
    free(path_copy);
    return -ENOENT;
  }

  /* 디렉터리의 직접 블록 해제 */
  uint8_t zero_block[SFUSE_BLOCK_SIZE] = {0};
  for (int i = 0; i < 12; ++i) {
    if (inode.direct[i]) {
      write_block(fs->backing_fd, inode.direct[i], zero_block);
      free_block(&fs->sb, &fs->bmaps->block,
                 inode.direct[i] - fs->sb.data_block_start);
      inode.direct[i] = 0;
    }
  }

  /* 아이노드 해제 및 초기화 */
  free_inode(&fs->sb, &fs->bmaps->inode, ino);
  struct sfuse_inode empty_inode = {0};
  inode_sync(fs->backing_fd, &fs->sb, ino, &empty_inode);

  /* 상위 디렉터리 메타데이터 갱신 */
  time_t now = time(NULL);
  parent_inode.mtime = (uint32_t)now;
  parent_inode.ctime = (uint32_t)now;
  inode_sync(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);

  free(path_copy);
  return 0;
}

/**
 * @brief 파일 또는 디렉터리 이름 변경 (이동)
 *
 * 지정된 경로의 엔트리를 원본 디렉터리에서 제거하고, 대상 디렉터리에 새
 * 엔트리를 추가한다.
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param from 원본 경로 (null-terminated 문자열)
 * @param to   대상 경로 (null-terminated 문자열)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_rename(struct sfuse_fs *fs, const char *from, const char *to) {
  uint32_t src_ino;
  if (fs_resolve_path(fs, from, &src_ino) < 0) {
    return -ENOENT;
  }
  uint32_t dummy;
  /* 대상이 이미 존재하면 실패 */
  if (fs_resolve_path(fs, to, &dummy) == 0) {
    return -EEXIST;
  }

  /* 경로 복사 및 분할 */
  char *from_copy = strdup(from);
  char *to_copy = strdup(to);
  if (!from_copy || !to_copy) {
    free(from_copy);
    free(to_copy);
    return -ENOMEM;
  }
  char *from_name = strrchr(from_copy, '/');
  char *to_name = strrchr(to_copy, '/');
  *from_name = '\0';
  *to_name = '\0';
  char *from_parent_path = (*from_copy == '\0') ? "/" : from_copy;
  char *to_parent_path = (*to_copy == '\0') ? "/" : to_copy;
  from_name++;
  to_name++;

  /* 부모 디렉터리 inode 조회 */
  uint32_t from_parent_ino, to_parent_ino;
  fs_resolve_path(fs, from_parent_path, &from_parent_ino);
  if (fs_resolve_path(fs, to_parent_path, &to_parent_ino) < 0) {
    free(from_copy);
    free(to_copy);
    return -ENOENT;
  }

  struct sfuse_inode from_parent_inode, to_parent_inode;
  inode_load(fs->backing_fd, &fs->sb, from_parent_ino, &from_parent_inode);
  inode_load(fs->backing_fd, &fs->sb, to_parent_ino, &to_parent_inode);
  /* 디렉터리 여부 확인 */
  if ((from_parent_inode.mode & S_IFDIR) == 0 ||
      (to_parent_inode.mode & S_IFDIR) == 0) {
    free(from_copy);
    free(to_copy);
    return -ENOTDIR;
  }

  uint8_t buf_block[SFUSE_BLOCK_SIZE];
  /* 원본 부모 디렉터리에서 엔트리 제거 */
  for (uint32_t i = 0; i < 12U; ++i) {
    if (from_parent_inode.direct[i] == 0)
      continue;
    read_block(fs->backing_fd, from_parent_inode.direct[i], buf_block);
    struct sfuse_dirent *entries = (struct sfuse_dirent *)buf_block;
    for (uint32_t j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode == src_ino &&
          strncmp(entries[j].name, from_name, SFUSE_NAME_MAX) == 0) {
        entries[j].inode = 0;
        entries[j].name[0] = '\0';
        write_block(fs->backing_fd, from_parent_inode.direct[i], entries);
        goto removed_src;
      }
    }
  }
removed_src:

  /* 대상 부모 디렉터리에 엔트리 추가 */
  for (uint32_t i = 0; i < 12U; ++i) {
    if (to_parent_inode.direct[i] == 0) {
      int new_blk = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_blk < 0)
        break;
      to_parent_inode.direct[i] = new_blk;
      struct sfuse_dirent *entries = (struct sfuse_dirent *)buf_block;
      memset(entries, 0, sizeof(buf_block));
      entries[0].inode = src_ino;
      strncpy(entries[0].name, to_name, SFUSE_NAME_MAX - 1);
      entries[0].name[SFUSE_NAME_MAX - 1] = '\0';
      write_block(fs->backing_fd, to_parent_inode.direct[i], entries);
      to_parent_inode.size += SFUSE_BLOCK_SIZE;
      break;
    } else {
      read_block(fs->backing_fd, to_parent_inode.direct[i], buf_block);
      struct sfuse_dirent *entries = (struct sfuse_dirent *)buf_block;
      for (uint32_t k = 0; k < DENTS_PER_BLOCK; ++k) {
        if (entries[k].inode == 0) {
          entries[k].inode = src_ino;
          strncpy(entries[k].name, to_name, SFUSE_NAME_MAX - 1);
          write_block(fs->backing_fd, to_parent_inode.direct[i], entries);
          goto added_dst;
        }
      }
    }
  }
added_dst:

  /* 메타데이터 갱신 */
  time_t now_time = time(NULL);
  from_parent_inode.mtime = from_parent_inode.ctime = (uint32_t)now_time;
  to_parent_inode.mtime = to_parent_inode.ctime = (uint32_t)now_time;

  struct sfuse_inode src_inode;
  inode_load(fs->backing_fd, &fs->sb, src_ino, &src_inode);
  src_inode.ctime = (uint32_t)now_time;

  inode_sync(fs->backing_fd, &fs->sb, src_ino, &src_inode);
  inode_sync(fs->backing_fd, &fs->sb, from_parent_ino, &from_parent_inode);
  inode_sync(fs->backing_fd, &fs->sb, to_parent_ino, &to_parent_inode);

  free(from_copy);
  free(to_copy);
  return 0;
}

/**
 * @brief 파일 크기 조정
 *
 * 지정된 경로의 파일 크기를 주어진 크기로 변경.
 * 크기 축소 시 불필요한 데이터 블록을 해제하고,
 * 크기 확장 시 필요한 경우 단일 바이트 쓰기를 통해 공간을 확보.
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param path 대상 파일 경로 (null-terminated 문자열)
 * @param size 새 파일 크기 (바이트 단위)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_truncate(struct sfuse_fs *fs, const char *path, off_t size) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }

  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);

  /* 디렉터리인 경우 오류 반환 */
  if ((inode.mode & S_IFDIR) == S_IFDIR) {
    return -EISDIR;
  }

  off_t old_size = inode.size;
  /* 변경할 크기가 동일하면 즉시 반환 */
  if (size == old_size) {
    return 0;
  }
  /* 크기 축소 */
  else if (size < old_size) {
    uint32_t keep_blocks =
        (size + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE; /**< 유지할 블록 수 */
    uint32_t old_blocks = (old_size + SFUSE_BLOCK_SIZE - 1) /
                          SFUSE_BLOCK_SIZE; /**< 기존 블록 수 */

    /* 축소되어야 할 블록 해제 */
    for (uint32_t i = keep_blocks; i < old_blocks; ++i) {
      if (i < 12) {
        if (inode.direct[i] != 0) {
          free_block(&fs->sb, &fs->bmaps->block, inode.direct[i]);
          inode.direct[i] = 0;
        }
      } else if (i < 12U + SFUSE_PTRS_PER_BLOCK) {
        if (inode.indirect != 0) {
          uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
          read_block(fs->backing_fd, inode.indirect, ptrs);
          int idx = i - 12;
          if (ptrs[idx] != 0) {
            free_block(&fs->sb, &fs->bmaps->block, ptrs[idx]);
            ptrs[idx] = 0;
          }
          write_block(fs->backing_fd, inode.indirect, ptrs);
        }
      } else {
        if (inode.double_indirect != 0) {
          uint32_t level1_arr[SFUSE_PTRS_PER_BLOCK];
          read_block(fs->backing_fd, inode.double_indirect, level1_arr);
          uint32_t idx = i - (12 + SFUSE_PTRS_PER_BLOCK);
          uint32_t l1_idx = idx / SFUSE_PTRS_PER_BLOCK; /**< 1차 인덱스 */
          uint32_t l2_idx = idx % SFUSE_PTRS_PER_BLOCK; /**< 2차 인덱스 */

          if (level1_arr[l1_idx] != 0) {
            uint32_t level2_arr[SFUSE_PTRS_PER_BLOCK];
            read_block(fs->backing_fd, level1_arr[l1_idx], level2_arr);
            if (level2_arr[l2_idx] != 0) {
              free_block(&fs->sb, &fs->bmaps->block, level2_arr[l2_idx]);
              level2_arr[l2_idx] = 0;
            }
            write_block(fs->backing_fd, level1_arr[l1_idx], level2_arr);
          }
        }
      }
    }

    /* 단일 간접 블록 전체가 비었는지 확인 후 해제 */
    if (inode.indirect != 0) {
      uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.indirect, ptrs);
      int all_empty = 1;
      for (uint32_t j = 0; j < SFUSE_PTRS_PER_BLOCK; ++j) {
        if (ptrs[j] != 0) {
          all_empty = 0;
          break;
        }
      }
      if (all_empty) {
        free_block(&fs->sb, &fs->bmaps->block, inode.indirect);
        inode.indirect = 0;
      }
    }

    /* 이중 간접 블록 전체가 비었는지 확인 후 해제 */
    if (inode.double_indirect != 0) {
      uint32_t level1[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.double_indirect, level1);
      int all_l1_empty = 1;
      for (uint32_t a = 0; a < SFUSE_PTRS_PER_BLOCK; ++a) {
        if (level1[a] != 0) {
          uint32_t level2[SFUSE_PTRS_PER_BLOCK];
          read_block(fs->backing_fd, level1[a], level2);
          int all_l2_empty = 1;
          for (uint32_t b = 0; b < SFUSE_PTRS_PER_BLOCK; ++b) {
            if (level2[b] != 0) {
              all_l2_empty = 0;
              break;
            }
          }
          if (all_l2_empty) {
            free_block(&fs->sb, &fs->bmaps->block, level1[a]);
            level1[a] = 0;
          } else {
            all_l1_empty = 0;
          }
        }
      }
      if (all_l1_empty) {
        free_block(&fs->sb, &fs->bmaps->block, inode.double_indirect);
        inode.double_indirect = 0;
      } else {
        write_block(fs->backing_fd, inode.double_indirect, level1);
      }
    }

    inode.size = size; /**< 파일 크기 갱신 */
  }
  /* 크기 확장 */
  else {
    char zero = 0;
    if (fs_write(fs, path, &zero, 1, size - 1) < 0) {
      return -EIO;
    }
    return 0;
  }

  /* 시간 정보 갱신 및 동기화 */
  time_t now_time = time(NULL);
  inode.mtime = (uint32_t)now_time; /**< 수정 시간 갱신 */
  inode.ctime = (uint32_t)now_time; /**< 변경 시간 갱신 */
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);
  return 0;
}

/**
 * @brief 파일 시스템 동기화 (flush/fsync)
 *
 * @param fs        파일 시스템 컨텍스트 포인터
 * @param path      대상 경로 (사용되지 않음)
 * @param datasync  데이터만 동기화할지 여부
 * @param fi        FUSE 파일 정보 구조체 (사용되지 않음)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_fsync(struct sfuse_fs *fs, const char *path, int datasync,
             struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  if (datasync) {
    if (fdatasync(fs->backing_fd) < 0)
      return -errno;
  } else {
    if (fsync(fs->backing_fd) < 0)
      return -errno;
  }
  return 0;
}

/**
 * @brief FUSE의 flush 호출 처리
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param path 대상 경로 (사용되지 않음)
 * @param fi   FUSE 파일 정보 구조체 (사용되지 않음)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_flush(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  if (fsync(fs->backing_fd) < 0)
    return -errno;
  return 0;
}

/**
 * @brief 파일 접근/수정 시간 업데이트
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param path 대상 경로 (null-terminated 문자열)
 * @param tv   tv[0]=접근 시간, tv[1]=수정 시간
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_utimens(struct sfuse_fs *fs, const char *path,
               const struct timespec tv[2]) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }
  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0) {
    return -EIO;
  }
  inode.atime = (uint32_t)tv[0].tv_sec; /**< 접근 시간 설정 */
  inode.mtime = (uint32_t)tv[1].tv_sec; /**< 수정 시간 설정 */
  inode.ctime = (uint32_t)time(NULL);   /**< 메타데이터 변경 시간 갱신 */
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);
  return 0;
}

/**
 * @brief 파일 접근 권한 검사
 *
 * @param fs   파일 시스템 컨텍스트 포인터
 * @param path 검사할 경로 (null-terminated 문자열)
 * @param mask 접근 마스크 (R_OK, W_OK, X_OK)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_access(struct sfuse_fs *fs, const char *path, int mask) {
  uint32_t ino;
  /* 경로 → inode 변환 실패 시 ENOENT 반환 */
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  /* mask에 따른 실제 권한 검사는 필요 시 구현 가능 */
  (void)mask;
  return 0;
}
