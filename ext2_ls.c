#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>
#include "ext2_helper.c"

// Note: that the argv include the file itself
// so when 2 input the expect argv should be 3
int main(int argc, char *argv[]) {
  int dir_tag = 0;
  unsigned char *disk;
  char * passed_path;
  char *current_path;
  char *image_path;
  int option_a = 0;
  // start error checking
  if (argc == 3) {
    // first one is the program name_len
    // second is the dick image file
    // third one is the file path
    current_path = argv[2];
    image_path = argv[1];
  } else if (argc == 4) {
    // two case here:
    // first one is the program name_len
    // second is the dick image file
    // third one is the file path
    // forth one is -a
    // or
    // first one is the program name_len
    // second is the dick image file
    // third one is -a
    // forth one is the file path
    image_path = argv[1];
    option_a = 1;
    // this is the first case
    if (strcmp("-a", argv[3]) == 0) {
      current_path = argv[2];
    } else if (strcmp("-a", argv[2]) == 0) {
      current_path = argv[3];
    } else {
      fprintf(stderr, "3Usage: %s <image file name> [-a] <absolute path>\n", argv[0]);
      exit(1);
    }
  } else {
    fprintf(stderr, "2Usage: %s <image file name> [-a] <absolute path>\n", argv[0]);
    exit(1);
  }
  if (current_path[0] != '/') {
    fprintf(stderr, "1Usage: %s <image file name> [-a] <absolute path>\n", argv[0]);
    exit(1);
  }
  // read the disk image
  disk = read_disk(image_path);
  // if it is look for a directory
  if (current_path[strlen(current_path) - 1] == '/') {
    dir_tag = 1;
  }
  // goto the target path in the disk image
  // 2. try to find the directory
  int inode_index = read_path (disk, current_path);
  if (inode_index == -1) {
    fprintf(stderr, "No such file or directory\n");
    exit(ENOENT);
  }
  struct ext2_inode *found_node = get_inode(disk, inode_index);
  // case file
  if (!S_ISDIR(found_node->i_mode)) {
    if (dir_tag == 1) {
      fprintf(stderr, "%s: cannot access %s : Not a directory\n", argv[0], current_path);
      exit(ENOENT);
    }
    passed_path = parse_path(current_path);
    char * prev;
    while (passed_path != NULL) {
      prev = passed_path;
      passed_path = strtok(NULL, "/");
    }
    fprintf(stdout, "%s", prev);
  } else {
    int i , block_number;
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // case directory
    // used data block
    int used_data_block = ((found_node->i_blocks)/(2<<sb->s_log_block_size));
    // to avoid go through extra block
    used_data_block = used_data_block > 12 ? used_data_block - 1: used_data_block;

    for (i = 0; i < used_data_block; i++ ) {
      if (i < 12) {
        // direct case
        block_number = found_node->i_block[i];
      } else {
        // indirect case
        unsigned int *in_dir = (unsigned int *) (disk + EXT2_BLOCK_SIZE * found_node->i_block[12] );
        block_number = in_dir[i - 12];
      }
      unsigned long pos = (unsigned long) disk + block_number * EXT2_BLOCK_SIZE;
      struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) (pos);
      // find the current dir entry table go through it and compare it
      // loop it by boundary
      // when the length gets reset to 0 .. either we reach the end of blocks
      // or we reach a empty dir entry and need to stop here
      printf("%s  %d\n", "block num" , block_number);
      do {
        int cur_len = dir_entry->rec_len;
        if (option_a == 1 ||
          (compare_path_name(".", dir_entry-> name, dir_entry->name_len) != 0 &&
          compare_path_name("..", dir_entry-> name, dir_entry->name_len) != 0)) {
            for (int i = 0; i < dir_entry->name_len; ++i) {
              char curr = (char) dir_entry->name[i];
              fprintf(stdout, "%c", curr);
            }
            if ((pos + cur_len) % EXT2_BLOCK_SIZE != 0) {
              fprintf(stdout, "\n") ;
            }
          }
          fflush(stdout);
          pos = pos + cur_len;
          dir_entry = (struct ext2_dir_entry_2 *) pos;

      } while ( pos % EXT2_BLOCK_SIZE != 0) ;
      if (i != used_data_block-1) {
        fprintf(stdout, "\n") ;
      }
    }
  }
  return 0;
}
