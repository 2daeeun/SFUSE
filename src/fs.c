// SPDX-License-Identifier: GPL-2.0
// SFUSE filesystem implementation

#include <errno.h> // 추가
#include <fuse3/fuse.h>
#include <stdbool.h> // 추가
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "bitmap.h"
#include "block.h"
#include "dir.h"
#include "fs.h"
#include "inode.h"
#include "super.h"

// main.c 에 선언된 플래그를 외부 참조
extern bool g_force_format;

// VSFS 최대 inode 개수 (super.h에도 정의되어 있을 수 있습니다)
#define SFUSE_MAX_INODES 1024

/**
 * 빈 이미지 파일을 VSFS 구조로 초기화(포맷)합니다.
 * - fd: 열린 이미지 파일 디스크립터
 * - sb: 수퍼블록 구조체를 채워놓을 버퍼
 */
static int fs_format_filesystem(int fd, struct sfuse_superblock *sb) {
  struct stat st;
  if (fstat(fd, &st) < 0)
    return -1;
  uint32_t total_all = st.st_size / SFUSE_BLOCK_SIZE;

  // 메타데이터 블록 수 계산
  uint32_t bm_blocks = (total_all > 32768 ? 2 : 1);
  uint32_t inodes_per_block = SFUSE_BLOCK_SIZE / sizeof(struct sfuse_inode);
  uint32_t it_blocks =
      (SFUSE_MAX_INODES + inodes_per_block - 1) / inodes_per_block;

  // 충분한 용량 확인
  if (total_all <= 1 + 1 + bm_blocks + it_blocks)
    return -1;

  // 수퍼블록 설정
  sb->magic = SFUSE_MAGIC;
  sb->total_inodes = SFUSE_MAX_INODES;
  sb->inode_bitmap_start = 1;
  sb->block_bitmap_start = 2;
  sb->inode_table_start = 2 + bm_blocks;
  sb->data_block_start = sb->inode_table_start + it_blocks;
  sb->total_blocks = total_all - sb->data_block_start;
  sb->free_inodes = SFUSE_MAX_INODES;
  sb->free_blocks = sb->total_blocks;

  // 비트맵 초기화
  struct sfuse_bitmaps bmaps;
  memset(&bmaps, 0, sizeof(bmaps));
  // inode 0,1 예약
  bmaps.inode.map[0] |= 0x03;
  sb->free_inodes -= 2;

  // 루트 inode 구성
  struct sfuse_inode root;
  memset(&root, 0, sizeof(root));
  root.mode = S_IFDIR | 0755;
  root.uid = getuid();
  root.gid = getgid();
  root.size = 0;
  time_t now = time(NULL);
  root.atime = root.mtime = root.ctime = (uint32_t)now;

  // 디스크에 기록
  // 1) 수퍼블록
  if (pwrite(fd, sb, SFUSE_BLOCK_SIZE, 0) != SFUSE_BLOCK_SIZE)
    return -1;
  // 2) inode 비트맵
  if (pwrite(fd, &bmaps.inode, SFUSE_BLOCK_SIZE,
             sb->inode_bitmap_start * SFUSE_BLOCK_SIZE) < 0)
    return -1;
  // 3) block 비트맵
  if (pwrite(fd, &bmaps.block, bm_blocks * SFUSE_BLOCK_SIZE,
             sb->block_bitmap_start * SFUSE_BLOCK_SIZE) < 0)
    return -1;
  // 4) 루트 inode
  if (inode_sync(fd, sb, 1, &root) < 0)
    return -1;

  return 0;
}

struct sfuse_fs *fs_initialize(const char *image_path, int *error_out) {
  struct sfuse_fs *fs = calloc(1, sizeof(struct sfuse_fs));
  if (!fs) {
    *error_out = ENOMEM;
    return NULL;
  }
  fs->backing_fd = open(image_path, O_RDWR);
  if (fs->backing_fd < 0) {
    *error_out = errno;
    free(fs);
    return NULL;
  }

  int r = sb_load(fs->backing_fd, &fs->sb);
  if (r < 0) {
    if (r == -EINVAL && g_force_format) {
      // 빈 이미지 자동 포맷
      fprintf(stderr, "SFUSE: 이미지 미포맷 감지, 자동 포맷 수행\n");
      if (fs_format_filesystem(fs->backing_fd, &fs->sb) < 0) {
        *error_out = EIO;
        close(fs->backing_fd);
        free(fs);
        return NULL;
      }
    } else {
      // 포맷 필요 없거나 다른 I/O 에러
      *error_out = (r == -EINVAL ? EINVAL : EIO);
      close(fs->backing_fd);
      free(fs);
      return NULL;
    }
  }

  // Allocate and load bitmaps
  fs->bmaps = malloc(sizeof(struct sfuse_bitmaps));
  if (!fs->bmaps) {
    *error_out = ENOMEM;
    close(fs->backing_fd);
    free(fs);
    return NULL;
  }
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

void fs_teardown(struct sfuse_fs *fs) {
  if (!fs)
    return;
  sb_sync(fs->backing_fd, &fs->sb);
  uint32_t bits_per_block = SFUSE_BLOCK_SIZE * 8;
  uint32_t im_blocks =
      (fs->sb.total_inodes + bits_per_block - 1) / bits_per_block;
  uint32_t bm_blocks =
      (fs->sb.total_blocks + bits_per_block - 1) / bits_per_block;
  bitmap_sync(fs->backing_fd, fs->sb.inode_bitmap_start, fs->bmaps,
              im_blocks + bm_blocks);
  free(fs->inode_table);
  free(fs->bmaps);
  close(fs->backing_fd);
  free(fs);
}

int fs_resolve_path(struct sfuse_fs *fs, const char *path, uint32_t *out_ino) {
  if (strcmp(path, "/") == 0) {
    *out_ino = 1; // root inode
    return 0;
  }
  char *path_copy = strdup(path);
  if (!path_copy)
    return -ENOMEM;
  uint32_t current = 1; // start at root
  char *token = strtok(path_copy, "/");
  while (token) {
    uint32_t next;
    int ret = dir_lookup(fs, current, token, &next);
    if (ret < 0) {
      free(path_copy);
      return ret;
    }
    current = next;
    token = strtok(NULL, "/");
  }
  free(path_copy);
  *out_ino = current;
  return 0;
}

int fs_getattr(struct sfuse_fs *fs, const char *path, struct stat *stbuf) {
  uint32_t ino;
  int ret = fs_resolve_path(fs, path, &ino);
  if (ret < 0)
    return ret;
  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;
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

int fs_readdir(struct sfuse_fs *fs, const char *path, void *buf,
               fuse_fill_dir_t filler, off_t offset) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }
  // List entries using helper
  return dir_list(fs, ino, buf, filler, offset);
}

int fs_open(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi) {
  (void)fi;
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }
  // No special state to set; just succeed if file exists
  return 0;
}

int fs_read(struct sfuse_fs *fs, const char *path, char *buf, size_t size,
            off_t offset) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;
  if ((inode.mode & S_IFDIR) == S_IFDIR)
    return -EISDIR;
  if ((size_t)offset >= inode.size)
    return 0;
  if ((size_t)offset + size > inode.size)
    size = inode.size - offset;
  size_t bytes_read = 0;
  uint8_t block_buf[SFUSE_BLOCK_SIZE];
  while (bytes_read < size) {
    uint32_t block_index = (offset + bytes_read) / SFUSE_BLOCK_SIZE;
    uint32_t block_offset = (offset + bytes_read) % SFUSE_BLOCK_SIZE;
    uint32_t to_read = SFUSE_BLOCK_SIZE - block_offset;
    if (to_read > size - bytes_read)
      to_read = size - bytes_read;
    uint32_t disk_block = 0;
    if (block_index < 12U) {
      disk_block = inode.direct[block_index];
    } else if (block_index < 12U + SFUSE_PTRS_PER_BLOCK) {
      if (!inode.indirect)
        break;
      uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
      if (read_block(fs->backing_fd, inode.indirect, ptrs) < 0)
        return -EIO;
      disk_block = ptrs[block_index - 12];
    } else {
      if (!inode.double_indirect)
        break;
      uint32_t l1[SFUSE_PTRS_PER_BLOCK];
      if (read_block(fs->backing_fd, inode.double_indirect, l1) < 0)
        return -EIO;
      uint32_t dbl_index = block_index - (12 + SFUSE_PTRS_PER_BLOCK);
      uint32_t l1_idx = dbl_index / SFUSE_PTRS_PER_BLOCK;
      uint32_t l2_idx = dbl_index % SFUSE_PTRS_PER_BLOCK;
      if (l1_idx >= SFUSE_PTRS_PER_BLOCK)
        break;
      if (l1[l1_idx] == 0)
        break;
      uint32_t l2[SFUSE_PTRS_PER_BLOCK];
      if (read_block(fs->backing_fd, l1[l1_idx], l2) < 0)
        return -EIO;
      disk_block = l2[l2_idx];
    }
    if (disk_block == 0) {
      memset(buf + bytes_read, 0, to_read);
    } else {
      if (read_block(fs->backing_fd, disk_block, block_buf) < 0)
        return -EIO;
      memcpy(buf + bytes_read, block_buf + block_offset, to_read);
    }
    bytes_read += to_read;
  }
  return bytes_read;
}

int fs_write(struct sfuse_fs *fs, const char *path, const char *buf,
             size_t size, off_t offset) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0)
    return -ENOENT;
  struct sfuse_inode inode;
  if (inode_load(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;
  if ((inode.mode & S_IFDIR) == S_IFDIR)
    return -EISDIR;
  size_t bytes_written = 0;
  uint8_t block_buf[SFUSE_BLOCK_SIZE];
  while (bytes_written < size) {
    off_t cur_offset = offset + bytes_written;
    uint32_t block_index = cur_offset / SFUSE_BLOCK_SIZE;
    uint32_t block_offset = cur_offset % SFUSE_BLOCK_SIZE;
    size_t to_write = SFUSE_BLOCK_SIZE - block_offset;
    if (to_write > size - bytes_written)
      to_write = size - bytes_written;
    uint32_t *target = NULL;
    uint32_t disk_block = 0;
    if (block_index < 12U) {
      target = &inode.direct[block_index];
    } else if (block_index < 12U + SFUSE_PTRS_PER_BLOCK) {
      if (inode.indirect == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0) {
          return -ENOSPC;
        }
        inode.indirect = (uint32_t)new_block;
        uint32_t tmp[SFUSE_PTRS_PER_BLOCK] = {0};
        write_block(fs->backing_fd, inode.indirect, tmp);
      }
      uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.indirect, ptrs);
      target = &ptrs[block_index - 12];
      disk_block = *target;
      if (disk_block == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0) {
          return -ENOSPC;
        }
        disk_block = (uint32_t)new_block;
        *target = disk_block;
        write_block(fs->backing_fd, inode.indirect, ptrs);
      }
    } else {
      uint32_t dbl_index = block_index - (12 + SFUSE_PTRS_PER_BLOCK);
      uint32_t l1_idx = dbl_index / SFUSE_PTRS_PER_BLOCK;
      uint32_t l2_idx = dbl_index % SFUSE_PTRS_PER_BLOCK;
      if (inode.double_indirect == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0) {
          return -ENOSPC;
        }
        inode.double_indirect = (uint32_t)new_block;
        uint32_t tmp[SFUSE_PTRS_PER_BLOCK] = {0};
        write_block(fs->backing_fd, inode.double_indirect, tmp);
      }
      uint32_t l1[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.double_indirect, l1);
      if (l1[l1_idx] == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0) {
          return -ENOSPC;
        }
        l1[l1_idx] = (uint32_t)new_block;
        uint32_t tmp[SFUSE_PTRS_PER_BLOCK] = {0};
        write_block(fs->backing_fd, l1[l1_idx], tmp);
        write_block(fs->backing_fd, inode.double_indirect, l1);
      }
      uint32_t l2[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, l1[l1_idx], l2);
      if (l2[l2_idx] == 0) {
        int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
        if (new_block < 0) {
          return -ENOSPC;
        }
        l2[l2_idx] = (uint32_t)new_block;
        write_block(fs->backing_fd, l1[l1_idx], l2);
      }
      disk_block = l2[l2_idx];
      target = &l2[l2_idx];
    }
    if (target && *target == 0) {
      int new_block = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_block < 0) {
        return -ENOSPC;
      }
      *target = (uint32_t)new_block;
      disk_block = *target;
    } else {
      disk_block = *target;
    }
    if (read_block(fs->backing_fd, disk_block, block_buf) < 0)
      return -EIO;
    memcpy(block_buf + block_offset, buf + bytes_written, to_write);
    if (write_block(fs->backing_fd, disk_block, block_buf) < 0)
      return -EIO;
    bytes_written += to_write;
  }
  if ((uint32_t)(offset + bytes_written) > inode.size)
    inode.size = offset + bytes_written;
  inode.mtime = time(NULL);
  inode.ctime = inode.mtime;
  if (inode_sync(fs->backing_fd, &fs->sb, ino, &inode) < 0)
    return -EIO;
  return bytes_written;
}

int fs_create(struct sfuse_fs *fs, const char *path, mode_t mode,
              struct fuse_file_info *fi) {
  (void)fi;
  uint32_t dummy;
  if (fs_resolve_path(fs, path, &dummy) == 0) {
    return -EEXIST;
  }
  // Separate parent directory and filename
  char *path_copy = strdup(path);
  if (!path_copy) {
    return -ENOMEM;
  }
  char *name = strrchr(path_copy, '/');
  if (!name || *(name + 1) == '\0') {
    free(path_copy);
    return -EINVAL;
  }
  *name = '\0';
  char *parent_path = (*path_copy == '\0') ? "/" : path_copy;
  name++;
  uint32_t parent_ino;
  if (fs_resolve_path(fs, parent_path, &parent_ino) < 0) {
    free(path_copy);
    return -ENOENT;
  }
  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  if ((parent_inode.mode & S_IFDIR) == 0) {
    free(path_copy);
    return -ENOTDIR;
  }
  // Allocate a new inode
  int new_ino = alloc_inode(&fs->sb, &fs->bmaps->inode);
  if (new_ino < 0) {
    free(path_copy);
    return new_ino;
  }
  struct sfuse_inode new_inode;
  memset(&new_inode, 0, sizeof(new_inode));
  new_inode.mode = (mode & 0xFFF) | S_IFREG;
  new_inode.uid = getuid();
  new_inode.gid = getgid();
  new_inode.size = 0;
  time_t now = time(NULL);
  new_inode.atime = (uint32_t)now;
  new_inode.mtime = (uint32_t)now;
  new_inode.ctime = (uint32_t)now;
  // Find a free directory entry in parent and add the new entry
  uint8_t dir_block[SFUSE_BLOCK_SIZE];
  int added = 0;
  for (uint32_t i = 0; i < 12U && !added; ++i) {
    if (parent_inode.direct[i] == 0) {
      // Allocate a new block for the directory entries
      int new_dir_block = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_dir_block < 0) {
        // Roll back inode allocation
        free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
        free(path_copy);
        return -ENOSPC;
      }
      parent_inode.direct[i] = new_dir_block;
      // Initialize new directory block
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
        entries[j].inode = 0;
        entries[j].name[0] = '\0';
      }
      // Place new entry at first position
      entries[0].inode = new_ino;
      strncpy(entries[0].name, name, SFUSE_NAME_MAX);
      entries[0].name[SFUSE_NAME_MAX - 1] = '\0'; /* null-termination 보장 */
      write_block(fs->backing_fd, parent_inode.direct[i], entries);
      parent_inode.size += SFUSE_BLOCK_SIZE;
      added = 1;
    } else {
      if (read_block(fs->backing_fd, parent_inode.direct[i], dir_block) < 0) {
        continue;
      }
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
        if (entries[j].inode == 0) {
          entries[j].inode = new_ino;
          strncpy(entries[j].name, name, SFUSE_NAME_MAX);
          entries[j].name[SFUSE_NAME_MAX - 1] =
              '\0'; /* ensure null-termination */
          write_block(fs->backing_fd, parent_inode.direct[i], entries);
          added = 1;
          break;
        }
      }
    }
  }
  if (!added) {
    // Directory is full (no free entry)
    // Roll back allocation
    free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
    free(path_copy);
    return -ENOSPC;
  }
  // Write new inode to disk
  inode_sync(fs->backing_fd, &fs->sb, new_ino, &new_inode);
  // Update parent directory metadata and sync
  parent_inode.mtime = (uint32_t)now;
  parent_inode.ctime = (uint32_t)now;
  inode_sync(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  free(path_copy);
  return 0;
}

int fs_mkdir(struct sfuse_fs *fs, const char *path, mode_t mode) {
  uint32_t dummy;
  if (fs_resolve_path(fs, path, &dummy) == 0) {
    return -EEXIST;
  }
  // Separate parent directory and name
  char *path_copy = strdup(path);
  if (!path_copy) {
    return -ENOMEM;
  }
  char *name = strrchr(path_copy, '/');
  if (!name || *(name + 1) == '\0') {
    free(path_copy);
    return -EINVAL;
  }
  *name = '\0';
  char *parent_path = (*path_copy == '\0') ? "/" : path_copy;
  name++;
  uint32_t parent_ino;
  if (fs_resolve_path(fs, parent_path, &parent_ino) < 0) {
    free(path_copy);
    return -ENOENT;
  }
  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  if ((parent_inode.mode & S_IFDIR) == 0) {
    free(path_copy);
    return -ENOTDIR;
  }
  // Allocate new inode for directory
  int new_ino = alloc_inode(&fs->sb, &fs->bmaps->inode);
  if (new_ino < 0) {
    free(path_copy);
    return new_ino;
  }
  struct sfuse_inode new_inode;
  memset(&new_inode, 0, sizeof(new_inode));
  new_inode.mode = (mode & 0xFFF) | S_IFDIR;
  new_inode.uid = getuid();
  new_inode.gid = getgid();
  new_inode.size = 0;
  time_t now = time(NULL);
  new_inode.atime = (uint32_t)now;
  new_inode.mtime = (uint32_t)now;
  new_inode.ctime = (uint32_t)now;
  // Add entry to parent directory
  uint8_t dir_block[SFUSE_BLOCK_SIZE];
  int added = 0;
  for (uint32_t i = 0; i < 12U && !added; ++i) {
    if (parent_inode.direct[i] == 0) {
      int new_dir_block = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_dir_block < 0) {
        free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
        free(path_copy);
        return -ENOSPC;
      }
      parent_inode.direct[i] = new_dir_block;
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
        entries[j].inode = 0;
        entries[j].name[0] = '\0';
      }
      entries[0].inode = new_ino;
      strncpy(entries[0].name, name, SFUSE_NAME_MAX);
      write_block(fs->backing_fd, parent_inode.direct[i], entries);
      parent_inode.size += SFUSE_BLOCK_SIZE;
      added = 1;
    } else {
      read_block(fs->backing_fd, parent_inode.direct[i], dir_block);
      struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
      for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
        if (entries[j].inode == 0) {
          entries[j].inode = new_ino;
          strncpy(entries[j].name, name, SFUSE_NAME_MAX);
          entries[j].name[SFUSE_NAME_MAX - 1] =
              '\0'; /* ensure null-termination */
          write_block(fs->backing_fd, parent_inode.direct[i], entries);
          added = 1;
          break;
        }
      }
    }
  }
  if (!added) {
    free_inode(&fs->sb, &fs->bmaps->inode, new_ino);
    free(path_copy);
    return -ENOSPC;
  }
  inode_sync(fs->backing_fd, &fs->sb, new_ino, &new_inode);
  parent_inode.mtime = (uint32_t)now;
  parent_inode.ctime = (uint32_t)now;
  inode_sync(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  free(path_copy);
  return 0;
}

int fs_unlink(struct sfuse_fs *fs, const char *path) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }
  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if ((inode.mode & S_IFDIR) == S_IFDIR) {
    return -EISDIR;
  }
  // Determine parent directory and filename
  char *path_copy = strdup(path);
  if (!path_copy) {
    return -ENOMEM;
  }
  char *name = strrchr(path_copy, '/');
  *name = '\0';
  char *parent_path = (*path_copy == '\0') ? "/" : path_copy;
  name++;
  uint32_t parent_ino;
  fs_resolve_path(fs, parent_path, &parent_ino);
  // Remove entry from parent directory
  uint8_t dir_block[SFUSE_BLOCK_SIZE];
  struct sfuse_inode parent_inode;
  inode_load(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  // Find and remove entry from parent directory
  for (uint32_t i = 0; i < 12U; ++i) {
    if (parent_inode.direct[i] == 0)
      continue;
    if (read_block(fs->backing_fd, parent_inode.direct[i], dir_block) < 0)
      continue;
    struct sfuse_dirent *entries = (struct sfuse_dirent *)dir_block;
    for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode == ino &&
          strncmp(entries[j].name, name, SFUSE_NAME_MAX) == 0) {
        entries[j].inode = 0;
        entries[j].name[0] = '\0';
        write_block(fs->backing_fd, parent_inode.direct[i], entries);
        goto found_entry;
      }
    }
  }
found_entry:
  // Free all blocks of the file
  for (int i = 0; i < 12; ++i) {
    if (inode.direct[i] != 0) {
      free_block(&fs->sb, &fs->bmaps->block, inode.direct[i]);
      inode.direct[i] = 0;
    }
  }
  if (inode.indirect != 0) {
    uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
    if (read_block(fs->backing_fd, inode.indirect, ptrs) >= 0) {
      for (int k = 0; k < SFUSE_PTRS_PER_BLOCK; ++k) {
        if (ptrs[k] != 0) {
          free_block(&fs->sb, &fs->bmaps->block, ptrs[k]);
          ptrs[k] = 0;
        }
      }
    }
    free_block(&fs->sb, &fs->bmaps->block, inode.indirect);
    inode.indirect = 0;
  }
  if (inode.double_indirect != 0) {
    uint32_t level1[SFUSE_PTRS_PER_BLOCK];
    if (read_block(fs->backing_fd, inode.double_indirect, level1) >= 0) {
      for (int a = 0; a < SFUSE_PTRS_PER_BLOCK; ++a) {
        if (level1[a] != 0) {
          uint32_t level2[SFUSE_PTRS_PER_BLOCK];
          if (read_block(fs->backing_fd, level1[a], level2) >= 0) {
            for (int b = 0; b < SFUSE_PTRS_PER_BLOCK; ++b) {
              if (level2[b] != 0) {
                free_block(&fs->sb, &fs->bmaps->block, level2[b]);
                level2[b] = 0;
              }
            }
          }
          free_block(&fs->sb, &fs->bmaps->block, level1[a]);
        }
      }
    }
    free_block(&fs->sb, &fs->bmaps->block, inode.double_indirect);
    inode.double_indirect = 0;
  }
  // Free the inode itself
  free_inode(&fs->sb, &fs->bmaps->inode, ino);
  struct sfuse_inode empty_inode;
  memset(&empty_inode, 0, sizeof(empty_inode));
  inode_sync(fs->backing_fd, &fs->sb, ino, &empty_inode);
  // Update parent directory metadata
  time_t now_time = time(NULL);
  parent_inode.mtime = (uint32_t)now_time;
  parent_inode.ctime = (uint32_t)now_time;
  inode_sync(fs->backing_fd, &fs->sb, parent_ino, &parent_inode);
  free(path_copy);
  return 0;
}

int fs_rmdir(struct sfuse_fs *fs, const char *path) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }
  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if ((inode.mode & S_IFDIR) == 0) {
    return -ENOTDIR;
  }
  // Check directory is empty (no entries)
  uint8_t block[SFUSE_BLOCK_SIZE];
  for (int i = 0; i < 12; ++i) {
    if (inode.direct[i] == 0)
      continue;
    if (read_block(fs->backing_fd, inode.direct[i], block) < 0)
      continue;
    struct sfuse_dirent *entries = (struct sfuse_dirent *)block;
    for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
      if (entries[j].inode != 0) {
        return -ENOTEMPTY;
      }
    }
  }
  // Use fs_unlink logic to remove directory entry and free inode & blocks
  char *dummy_path = strdup(path);
  if (!dummy_path)
    return -ENOMEM;
  int res = fs_unlink(fs, path);
  free(dummy_path);
  return res;
}

int fs_rename(struct sfuse_fs *fs, const char *from, const char *to) {
  uint32_t src_ino;
  if (fs_resolve_path(fs, from, &src_ino) < 0) {
    return -ENOENT;
  }
  uint32_t dummy;
  if (fs_resolve_path(fs, to, &dummy) == 0) {
    return -EEXIST;
  }
  // Parse paths
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
  if ((from_parent_inode.mode & S_IFDIR) == 0 ||
      (to_parent_inode.mode & S_IFDIR) == 0) {
    free(from_copy);
    free(to_copy);
    return -ENOTDIR;
  }
  // Remove entry from source parent
  uint8_t buf_block[SFUSE_BLOCK_SIZE];
  for (uint32_t i = 0; i < 12U; ++i) {
    if (from_parent_inode.direct[i] == 0)
      continue;
    read_block(fs->backing_fd, from_parent_inode.direct[i], buf_block);
    struct sfuse_dirent *entries = (struct sfuse_dirent *)buf_block;
    for (int j = 0; j < DENTS_PER_BLOCK; ++j) {
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
  // Add entry to target parent
  for (uint32_t i = 0; i < 12U; ++i) {
    if (to_parent_inode.direct[i] == 0) {
      // allocate new block if needed
      int new_blk = alloc_block(&fs->sb, &fs->bmaps->block);
      if (new_blk < 0)
        break;
      to_parent_inode.direct[i] = new_blk;
      struct sfuse_dirent *entries = (struct sfuse_dirent *)buf_block;
      for (int k = 0; k < DENTS_PER_BLOCK; ++k) {
        entries[k].inode = 0;
        entries[k].name[0] = '\0';
      }
      entries[0].inode = src_ino;
      strncpy(entries[0].name, to_name, SFUSE_NAME_MAX);
      entries[0].name[SFUSE_NAME_MAX - 1] = '\0'; /* ensure null-termination */
      write_block(fs->backing_fd, to_parent_inode.direct[i], entries);
      to_parent_inode.size += SFUSE_BLOCK_SIZE;
      break;
    } else {
      read_block(fs->backing_fd, to_parent_inode.direct[i], buf_block);
      struct sfuse_dirent *entries = (struct sfuse_dirent *)buf_block;
      for (int k = 0; k < DENTS_PER_BLOCK; ++k) {
        if (entries[k].inode == 0) {
          entries[k].inode = src_ino;
          strncpy(entries[k].name, to_name, SFUSE_NAME_MAX);
          write_block(fs->backing_fd, to_parent_inode.direct[i], entries);
          goto added_dst;
        }
      }
    }
  }
added_dst:
  // Update metadata
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

int fs_truncate(struct sfuse_fs *fs, const char *path, off_t size) {
  uint32_t ino;
  if (fs_resolve_path(fs, path, &ino) < 0) {
    return -ENOENT;
  }
  struct sfuse_inode inode;
  inode_load(fs->backing_fd, &fs->sb, ino, &inode);
  if ((inode.mode & S_IFDIR) == S_IFDIR) {
    return -EISDIR;
  }
  off_t old_size = inode.size;
  if (size == old_size) {
    return 0;
  } else if (size < old_size) {
    // Shrink file
    uint32_t keep_blocks = (size + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE;
    uint32_t old_blocks = (old_size + SFUSE_BLOCK_SIZE - 1) / SFUSE_BLOCK_SIZE;
    // Free blocks beyond keep_blocks-1
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
          uint32_t l1_idx = idx / SFUSE_PTRS_PER_BLOCK;
          uint32_t l2_idx = idx % SFUSE_PTRS_PER_BLOCK;
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
    // If indirect block became empty, free it
    if (inode.indirect != 0) {
      uint32_t ptrs[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.indirect, ptrs);
      int all_empty = 1;
      for (int j = 0; j < SFUSE_PTRS_PER_BLOCK; ++j) {
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
    // If double-indirect became empty, free it
    if (inode.double_indirect != 0) {
      uint32_t level1[SFUSE_PTRS_PER_BLOCK];
      read_block(fs->backing_fd, inode.double_indirect, level1);
      int all_l1_empty = 1;
      for (int a = 0; a < SFUSE_PTRS_PER_BLOCK; ++a) {
        if (level1[a] != 0) {
          uint32_t level2[SFUSE_PTRS_PER_BLOCK];
          read_block(fs->backing_fd, level1[a], level2);
          int all_l2_empty = 1;
          for (int b = 0; b < SFUSE_PTRS_PER_BLOCK; ++b) {
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
    inode.size = size;
  } else {
    // Extend file by writing a byte at new_size-1
    char zero = 0;
    if (fs_write(fs, path, &zero, 1, size - 1) < 0) {
      return -EIO;
    }
    // After fs_write, inode size should be updated to new size
    return 0;
  }
  time_t now_time = time(NULL);
  inode.mtime = (uint32_t)now_time;
  inode.ctime = (uint32_t)now_time;
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);
  return 0;
}

/**
 * @brief Synchronize file data and metadata to disk.
 *
 * @param fs 파일 시스템 컨텍스트
 * @param path 파일 경로 (사용되지 않음)
 * @param datasync 데이터만 동기화할지 여부
 * @param fi FUSE 파일 정보 (사용되지 않음)
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
 * @brief 파일 데이터와 메타데이터를 디스크에 기록(flush)
 *
 * @param fs 파일 시스템 컨텍스트
 * @param path 파일 경로(사용되지 않음)
 * @param fi FUSE 파일 정보(사용되지 않음)
 * @return 성공 시 0, 실패 시 음수 오류 코드
 */
int fs_flush(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  if (fsync(fs->backing_fd) < 0)
    return -errno;
  return 0;
}

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
  inode.atime = (uint32_t)tv[0].tv_sec;
  inode.mtime = (uint32_t)tv[1].tv_sec;
  inode.ctime = (uint32_t)time(NULL);
  inode_sync(fs->backing_fd, &fs->sb, ino, &inode);
  return 0;
}

int fs_flush(struct sfuse_fs *fs, const char *path, struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  // Flush backing file (ensure data is written to disk)
  return (fsync(fs->backing_fd) < 0) ? -errno : 0;
}

int fs_fsync(struct sfuse_fs *fs, const char *path, int datasync,
             struct fuse_file_info *fi) {
  (void)path;
  (void)fi;
  if (datasync) {
    return (fdatasync(fs->backing_fd) < 0) ? -errno : 0;
  } else {
    return (fsync(fs->backing_fd) < 0) ? -errno : 0;
  }
}
