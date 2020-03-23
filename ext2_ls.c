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

/**
get inode from the disk by inode index
**/
struct ext2_inode *get_inode (unsigned char * disk, int inode_index) {
  // stuff from tut exercise
  // 1. get group descriptor
  struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  // 2. get inode_table
  struct ext2_inode * inode_table = (struct ext2_inode *) (disk + bgd->bg_inode_table * EXT2_BLOCK_SIZE);
  // 3. find the target inode, Note -1 is need here it is start at one by
  // when we store it we start at zero, so -1 offset is need here
  return (struct ext2_inode *)inode_table + inode_index - 1;
}



int compare_path_name (char *s1, char *s2, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (s2[i] == '\0' || s1[i] != s2[i]) {
            return 1;
        }
    }
    if (s2[i] == '\0') {
      return 0;
    } else {
      return 1;
    }
}

/**
read the path and try to find its target inode from disk
and return the inode number
**/
int read_path(unsigned char* disk, char* path) {
  struct ext2_inode * current_inode;
  int block_number;
  int current_inode_index = EXT2_ROOT_INO;
  char * current = path;
  int i;
  int next_index;
  // note: that the first one is . which means Root
  if ( path == NULL) {
    // it is at root path
    return current_inode_index;
  }

  // remember we still need to handle the end case
  while (current != NULL) {
      char* temp = current;
      // first the inode
      current_inode = get_inode(disk, current_inode_index );
      // find out what is type of inode it is, code from tut
      char type = (S_ISDIR(current_inode->i_mode)) ? 'd' : ((S_ISREG(current_inode->i_mode)) ? 'f' : 's');
      // if the current node is looking for file
      if (type == 'd') {
        // for each blocks, node we starts at 12
        for (i = 0; i < current_inode->i_blocks; i++ ) {
          if (i < 12) {
            // direct case
            block_number = current_inode->i_block[i];
          } else {
            // indirect case
            int *in_dir = (int *) (disk + EXT2_BLOCK_SIZE * current_inode->i_block[12]);
            block_number = in_dir[i - 12];
          }
          struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_number);
          // find the current dir entry table go through it and compare it
          // loop it by boundary
          next_index = -1;
          // printf("%d %d %s  %d ddss\n", next_index, current_inode_index, current, temp );
          do {
            // remove the ending / from the element
            if (temp[strlen(temp) - 1] == '/') {
                temp[strlen(temp) - 1] = 0;
            }
             // printf("%s %s %d %d %d\n", temp, dir_entry->name, dir_entry->inode, strcmp(temp, dir_entry->name), compare_path_name(dir_entry->name, temp, dir_entry->name_len) );
            if (compare_path_name(dir_entry->name, temp, dir_entry->name_len) == 0) {
              next_index = dir_entry->inode;
            }
            dir_entry = (struct ext2_dir_entry_2 *) (((unsigned long)dir_entry)
            + dir_entry->rec_len);
          } while (((unsigned long)dir_entry) % EXT2_BLOCK_SIZE != 0);
          // found the targe next
          if (next_index != -1) {
            current_inode_index = next_index;
            break;
          }
        }
        if (next_index == -1) {
          return -1;
        }
        current = strtok(NULL, "/");
        // printf("%d %s %s  ddss\n", current_inode_index, current, temp );
      }
      // it is look for
      if (type != 'd') {
        // error checking: file/dir is invalid
        if (current != NULL) {
          return -1;
        }
      }
  }
  return current_inode_index;
}

/*
Note if the last is / we need to handle it specially
Note: the passed in str will be modified
*/
char* parse_path(char* target_path) {
  char * temp;
  char* token = strtok(target_path, "/");
  return token;
}


/*
This function read the disk and return the disk
*/
unsigned char * read_disk(char *image_path) {
  unsigned char *disk;
  int fd = open(image_path,  O_RDWR);
  disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if(disk == MAP_FAILED) {
      perror("mmap");
      exit(1);
  }
  return disk;
}
// Note: that the argv include the file itself
// so when 2 input the expect argv should be 3
int main(int argc, char *argv[]) {
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
      fprintf(stderr, "1Usage: %s <image file name> [-a] <absolute path>\n", argv[0]);
      exit(1);
    }
  } else {
    fprintf(stderr, "2Usage: %s <image file name> [-a] <absolute path>\n", argv[0]);
    exit(1);
  }
  // read the disk image
  disk = read_disk(image_path);
  // goto the target path in the disk image
  // 1. parse the path in to directory
  char* tempstr = calloc(strlen(current_path)+1, sizeof(char));
  strcpy(tempstr, current_path);
  passed_path = parse_path(current_path);

  // char* prev = passed_path;
  // while (prev != NULL) {
  //   printf("%s dd\n", prev);
  //   prev = strtok(NULL, "/");
  // }
  // 2. try to find the directory
  int inode_index = read_path (disk, passed_path);
  if (inode_index == -1) {
    // something went wrong
    fprintf(stderr, "No such file or directory\n");
    exit(ENOENT);
  }
  struct ext2_inode *found_node = get_inode(disk, inode_index);
  // two case to handle here..
  // case file
  if (!S_ISDIR(found_node->i_mode)) {
    passed_path = parse_path(tempstr);
    char * prev;
    while (passed_path != NULL) {
      prev = passed_path;
      passed_path = strtok(NULL, "/");
    }
    fprintf(stdout, "%s\n", prev);
  } else {
    int i , block_number;
    // case directory
    for (i = 0; i < found_node->i_blocks; i++ ) {
      if (i < 12) {
        // direct case
        block_number = found_node->i_block[i];
      } else {
        // indirect case
        int *in_dir = (int *) (disk + EXT2_BLOCK_SIZE * found_node->i_block[12] );
        block_number = in_dir[i - 12];
      }
      struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) (disk + EXT2_BLOCK_SIZE * block_number);
      // find the current dir entry table go through it and compare it
      // loop it by boundary
      // when the length gets reset to 0 .. either we reach the end of blocks
      // or we reach a empty dir entry and need to stop here
      do {
        if (option_a == 1 ||
          (compare_path_name(".", dir_entry-> name, dir_entry->name_len) != 0 &&
          compare_path_name("..", dir_entry-> name, dir_entry->name_len) != 0)) {
            for (int i = 0; i < dir_entry->name_len; ++i) {
              char curr = (char) dir_entry->name[i];
              fprintf(stdout, "%c", curr);
            }
            if (dir_entry->name_len > 0) {
              fprintf(stdout, "\n") ;
            }
          }
          fflush(stdout);
          dir_entry =  (struct ext2_dir_entry_2 *) (((unsigned long)dir_entry)
          + dir_entry->rec_len);
      } while (((unsigned long)dir_entry) % EXT2_BLOCK_SIZE != 0) ;
    }
  }
  return 0;
}
