#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include "ext2.h"
#include <errno.h>

int check_valid_path(unsigned char *disk, char *path);
int check_valid_file(unsigned char *disk, int dir_inodenum, char *file_name);
struct ext2_inode *get_inode (unsigned char *disk, int inodenum);
int allocate_block(unsigned char *disk);
int allocate_inode(unsigned char *disk, int file_size, char *file_name);
int allocate_dirent(unsigned char *disk, char *file_name, int allocated_inodenum, int dir_inodenum);
char* get_file_name(char *file_path);
char* readFileBytes(const char *name);
int num_free_blocks (unsigned char *disk);
int num_free_inodes (unsigned char *disk);
int check_blocks(unsigned char *disk, int file_size);
int get_rec_len(int name_len);

char* get_file_name(char *file_path) {
   char *token = strtok(file_path, "/");
   char *ret;
   /* walk through other tokens */
   while( token != NULL ) {
      ret = token;
      token = strtok(NULL, "/");
   }
   return ret;
}

struct ext2_inode *get_inode (unsigned char *disk, int inodenum) {
  // stuff from tut exercise
  // 1. get group descriptor
  struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  // 2. find the target inode, Note -1 is need here it is start at one by
  // when we store it we start at zero, so -1 offset is need here
  return (struct ext2_inode *)(disk + bgd->bg_inode_table * EXT2_BLOCK_SIZE) + inodenum - 1;
}

int num_free_blocks (unsigned char *disk) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    return sb->s_free_blocks_count;
}

int num_free_inodes (unsigned char *disk) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    return sb->s_free_inodes_count;
}

char* readFileBytes(const char *name)
{
    FILE *fl = fopen(name, "r");
    fseek(fl, 0, SEEK_END);
    long len = ftell(fl);
    char *ret = (char *) malloc(len);
    fseek(fl, 0, SEEK_SET);
    fread(ret, 1, len, fl);
    fclose(fl);
    return ret;
}

int check_blocks(unsigned char *disk, int file_size) {
    int needed_blocks = file_size / 1024;
    if (file_size % 1024 != 0) {
        needed_blocks++;
    }
    if (needed_blocks <= num_free_blocks(disk)) {
        return 1;
    }
    return 0;
}

int check_valid_path(unsigned char *disk, char *path) {
    if (path[0] != '/') {
        return -ENOENT;
    }
    int inodenum = EXT2_ROOT_INO;
    int found_inode;
    char *splitted_path;
    int wrong_path = 0;
    splitted_path = strtok(path, "/");
    while (splitted_path != NULL && wrong_path == 0) {
        printf("working on directory %s\n", splitted_path);
        struct ext2_inode* curr_inode = get_inode(disk, inodenum);
        unsigned int *arr = curr_inode->i_block;
        found_inode = 0;
        while(*arr != 0){
            int blocknum = *arr;
            unsigned long pos = (unsigned long) disk + blocknum * EXT2_BLOCK_SIZE;
            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) pos;
            do {
                int cur_len = dir->rec_len;
                char type = (dir->file_type == EXT2_FT_REG_FILE) ? 'f' :
                            ((dir->file_type == EXT2_FT_DIR) ? 'd' : 's');
                char *name = dir->name;
                printf("directory name is %s\n", name);
                printf("directory inode is %d\n", dir->inode);
                if (type == 'd' && (strcmp(name, splitted_path) == 0)) {
                    printf("found dir\n");
                    // found the corresponding directory
                    // set inodenum to this directory's inode
                    // set found_inode to be 1 to break out of the current while loop
                    // to move on to the next inode dirent
                    inodenum = dir->inode;
                    found_inode = 1;
                }
                // Update position and index into it
                pos = pos + cur_len;
                dir = (struct ext2_dir_entry_2 *) pos;
                // Last directory entry leads to the end of block. Check if
                // Position is multiple of block size, means we have reached the end
            } while((pos % EXT2_BLOCK_SIZE != 0) && (found_inode == 0));
            if (found_inode == 0) {
                printf("%s", "did not find directory\n");
                wrong_path = 1;
                return -1;
            }
            *arr++;
        }
        splitted_path = strtok(NULL, "/");
    }
    return inodenum;
}

// given file name and its directory inodenumber, check if file_name is within the direcotry already
// return 1 if it does not exist, -1 o/w.
int check_valid_file(unsigned char *disk, int dir_inodenum, char *file_name) {
    printf("\n\n\n");
    printf("file name to check is %s\n", file_name);
    struct ext2_inode* dir_inode = get_inode(disk, dir_inodenum);
    unsigned int *arr = dir_inode->i_block;
     while(*arr != 0){
        int blocknum = *arr;
        unsigned long pos = (unsigned long) disk + blocknum * EXT2_BLOCK_SIZE;
        struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) pos;
        do {
            int cur_len = dir->rec_len;
            char *name = dir->name;
            printf("directory name is %s\n", name);
            //printf("directory type is %s\n", type);
            printf("directory inode is %d\n", dir->inode);
            if ((strcmp(name, file_name) == 0)) {
                printf("found a file/dir with file_name\n");
                // found a file/dir with the same name
                // return -1.
                return -1;
            }
            // Update position and index into it
            pos = pos + cur_len;
            dir = (struct ext2_dir_entry_2 *) pos;
            // Last directory entry leads to the end of block. Check if
            // Position is multiple of block size, means we have reached the end
        } while(pos % EXT2_BLOCK_SIZE != 0);
        *arr++;
     }
    return 1;
}

int get_rec_len(int name_len) {
    int padding = 4 - (name_len % 4);
    return 8 + name_len + padding;
}

int allocate_dirent(unsigned char *disk, char *file_name, int allocated_inodenum, int dir_inodenum) {
    struct ext2_inode* dir_inode = get_inode(disk, dir_inodenum);
    // init a new dir_ent struct
    struct ext2_dir_entry_2 *new_dirent = (struct ext2_dir_entry_2*)malloc(sizeof(struct ext2_dir_entry_2));
    new_dirent->file_type = EXT2_FT_REG_FILE;
    new_dirent->name_len = strlen(file_name);
    int new_ent_reclen = get_rec_len(strlen(file_name));
    unsigned int *arr = dir_inode->i_block;
    int blocknum = *arr;
    unsigned long pos = (unsigned long) disk + blocknum * EXT2_BLOCK_SIZE;
    struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) pos;
    // looping all the entries within the directory to find the last entry in the directory
    do {
        int cur_len = dir->rec_len;
        // Update position and index into it
        pos = pos + cur_len;
        // check before changing dir so that it does not change when position is at the 0th of the next block
        if (pos % EXT2_BLOCK_SIZE != 0) {
            dir = (struct ext2_dir_entry_2 *) pos;
        }
        // Last directory entry leads to the end of block. Check if
        // Position is multiple of block size, means we have reached the end
    } while((pos % EXT2_BLOCK_SIZE != 0));
    // now dir has the last entry, change its rec_len to its own rec_len
    // change new_dirent to suffice 1024 bytes
    int prev_ent_reclen = dir->rec_len;
    printf("prev ent rec len is %d\n", prev_ent_reclen);
    int prev_ent_actual_reclen = get_rec_len(dir->name_len);
    dir->rec_len = prev_ent_actual_reclen;
    // position that the prev dir entry is at
    pos = pos - prev_ent_reclen;
    // new pos for the new entry
    pos = pos + prev_ent_actual_reclen;
    new_dirent->rec_len = prev_ent_reclen - prev_ent_actual_reclen;
    printf("new ent rec len is %d\n", new_dirent->rec_len);
    new_dirent->inode = allocated_inodenum;
    strncpy(new_dirent->name, file_name, strlen(file_name));
    dir = (struct ext2_dir_entry_2 *) pos;
    memcpy(dir, new_dirent, new_ent_reclen);
    return 0;
}

int allocate_block(unsigned char *disk) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
    // get the block bitmap and find the index of the free block
    int index2 = 0;
    char *bm = (char *) (disk + (bgd->bg_block_bitmap * EXT2_BLOCK_SIZE));
    for (int i = 0; i < sb->s_blocks_count; i++) {
        unsigned c = bm[i / 8];                     // get the corresponding byte
        int not_used = (c & (1 << index2)) == 0;
        // have found the index of a not used block
        if (not_used) {
            unsigned char *target_block_byte = bm + i / 8;
            *target_block_byte |= 1 << (i % 8);
            printf("have found a free block at blocknum: %d\n", i+1);
            return i + 1;
        }
        if (++index2 == 8) (index2 = 0); // increment shift index, if > 8 reset.
    }
    return -1;
}


// return the inodenum of the allocated inode, -1 if failed to allocate blocks/inode
int allocate_inode(unsigned char *disk, int file_size, char *file_name) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // get the inode bitmap to manipulate its bits if there are free inode in the bitmap
    struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
    char *bmi = (char *) (disk + (bgd->bg_inode_bitmap * EXT2_BLOCK_SIZE));
    int index2 = 0;
    if (sb->s_free_inodes_count > 0) {
        for (int i = 0; i < sb->s_inodes_count; i++) {
        unsigned c = bmi[i / 8];                     // get the corresponding byte
        int not_used = (c & (1 << index2)) == 0;
        if (not_used && i > 10) {
            printf("have found a free inode at inodenum %d\n", i+1);
            // found a free inode in the bitmap
            // if there are enough free blocks, allocate the data blocks for the file
            // if successful, change inode bitmap and decrease super block and group desc free inode count
            int has_enough_blocks = check_blocks(disk, file_size);
            printf("has enough blocks? %d\n", has_enough_blocks);
            if (has_enough_blocks) {
                // block_count as well as going to be the index for i->block
                int block_count = 0;
                // the inodenum to be returned
                int inodenum = i + 1;
                // number of blocks used by this inode
                int blocks_count = 0 ;
                // initialize inode struct
                struct ext2_inode* new_inode = (struct ext2_inode*)malloc(sizeof(struct ext2_inode));
                // read the contents from the file
                char tmp_buffer[EXT2_BLOCK_SIZE+1];
                FILE *fp;
                fp = fopen(file_name, "r");
                while( fgets(tmp_buffer, EXT2_BLOCK_SIZE+1, fp) != NULL )
                {
                    // if the block is saved not in indirect block
                    if (block_count < 12) {
                        new_inode->i_block[block_count] = allocate_block(disk);
                        blocks_count += 2;
                        unsigned int *block = (unsigned int *)(disk + (new_inode->i_block[block_count]) * EXT2_BLOCK_SIZE);
                        memcpy(block, tmp_buffer, EXT2_BLOCK_SIZE);
                    } else {
                        // if saving in the indirect block now
                        new_inode->i_block[12] = allocate_block(disk);
                        unsigned int *indirect_block = (unsigned int *)(disk + (new_inode->i_block[12]) * EXT2_BLOCK_SIZE);
                        blocks_count += 2;
                        // process first block, then read file again for next blocks
                        int new_blocknum = allocate_block(disk);
                        indirect_block[0] = new_blocknum;
                        unsigned int *block = (unsigned int *)(disk + new_blocknum * EXT2_BLOCK_SIZE);
                        blocks_count += 2;
                        memcpy(block, tmp_buffer, EXT2_BLOCK_SIZE);
                        int indirect_block_count = 1;
                        while( fgets(tmp_buffer, EXT2_BLOCK_SIZE+1, fp) != NULL ) {
                            int new_blocknum = allocate_block(disk);
                            indirect_block[indirect_block_count] = new_blocknum;
                            unsigned int *block = (unsigned int *)(disk + new_blocknum * EXT2_BLOCK_SIZE);
                            memcpy(block, tmp_buffer, EXT2_BLOCK_SIZE);
                            blocks_count += 2;
                            indirect_block_count++;
                        }
                    }
                    block_count++;
                }
                // filling up the bitmap and the inode struct
                unsigned char *target_inode_byte = bmi + i / 8;
                *target_inode_byte |= 1 << (i % 8);
                bgd->bg_free_inodes_count--;
                sb->s_free_inodes_count--;
                i = sb->s_inodes_count;
                new_inode->i_links_count = 1;
                new_inode->i_mode |= EXT2_S_IFREG;
                new_inode->i_blocks = blocks_count;
                new_inode->i_size = file_size;
                // memcpy the new inode into memory
                struct ext2_inode *inode = get_inode(disk, inodenum);
                memcpy(inode, new_inode, sizeof(struct ext2_inode));
                return inodenum;
            }
        }
        //printf("%d", (c & (1 << index2)) == 0);       // Print the correcponding bit
        // If that bit was a 1, inode is used, store it into the array.
        // Note, this is the index number, NOT the inode number
        // inode number = index number + 1
        if (++index2 == 8) (index2 = 0); // increment shift index, if > 8 reset.
    };
    } else {
        perror("Theres no enough inode space");
        return -1;
    }
    return -1;
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
mode 0 => just find the indoe number for either file or folder
mode 1 => return ENOENT for the not found directory
mode 2 => return ENOENT for the not found file
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
          // Get the position in bytes and index to block
          unsigned long pos = (unsigned long) disk + block_number * EXT2_BLOCK_SIZE;
          struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) pos;
          next_index = -1;
          do {
              // Get the length of the current block and type
              int cur_len = dir->rec_len;
              if (temp[strlen(temp) - 1] == '/') {
                  temp[strlen(temp) - 1] = 0;
              }
               // printf("%s %s %d %d %d\n", temp, dir_entry->name, dir_entry->inode, strcmp(temp, dir_entry->name), compare_path_name(dir_entry->name, temp, dir_entry->name_len) );
              if (compare_path_name(dir->name, temp, dir->name_len) == 0) {
                next_index = dir->inode;
              }
              // Update position and index into it
              pos = pos + cur_len;
              dir = (struct ext2_dir_entry_2 *) pos;
              // Last directory entry leads to the end of block. Check if
              // Position is multiple of block size, means we have reached the end
          } while (pos % EXT2_BLOCK_SIZE != 0);
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
  // struct ext2_inode *result_inode = get_inode(disk, current_inode_index);
  // if (mode == 1) {
  //
  // } else if ( mode == 2) {
  //
  // }
  return current_inode_index;
}

/*
Note if the last is / we need to handle it specially
Note: the passed in str will be modified
*/
char* parse_path(char* target_path) {
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



/**
* Returns the proper rec_len size for a directory entry, aligned
* to 4 bytes.
*/
int get_min_rec_len(int name_len) {
   int padding = 4 - (name_len % 4);
   return 8 + name_len + padding;
}




/**
mode 0 is the for inode
mode 1 is for the block

**/
int set_bitmap(int mode, unsigned char *disk, int index, int value) {
  char* bitmap;
  struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  // get the inode bitmap
  if (mode == 0) {
    bitmap = (char *) (disk + (bgd->bg_inode_bitmap * EXT2_BLOCK_SIZE));
  } else {
    bitmap = (char *) (disk + (bgd->bg_block_bitmap * EXT2_BLOCK_SIZE));
  }
  index--;
  unsigned c = bitmap[index / 8];
  if (value == 1) {
    bitmap[index / 8] = c | (1 << (index % 8));
  } else {
    bitmap[index / 8] &= ~(1 << (index % 8));
  }
  return 1;
}


int find_free_inode(unsigned char *disk) {
  // use stuff from tut ex
  struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  // get the inode bitmap
  char *bmi = (char *) (disk + (bgd->bg_inode_bitmap * EXT2_BLOCK_SIZE));
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  if (bgd->bg_free_inodes_count == 0) {
    return -1;
  } else {
    int index2 = 0;
    for (int i = 0; i < sb->s_inodes_count; i++) {
        unsigned c = bmi[i / 8];                     // get the corresponding byte
        unsigned status = ((c & (1 << index2)) > 0);       // Print the correcponding bit
        // If that bit was a 1, inode is used, store it into the array.
        // Note, this is the index number, NOT the inode number
        // inode number = index number + 1
        if (status == 0 && i > 10) {    // > 10 because first 11 not used
            return i + 1;
        }
        if (++index2 == 8) (index2 = 0); // increment shift index, if > 8 reset.
    }
    return -1;
  }
}

int *find_free_blocks(unsigned char *disk, int require_block) {
  // init array to hold used block
  int * res = (int *)malloc(sizeof(int) * require_block) ;
  if (res == NULL) {
      fprintf(stderr, "no memory!\n");
      exit(1);
  }
  // used code from tut ex
  struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  char *bm = (char *) (disk + (bgd->bg_block_bitmap * EXT2_BLOCK_SIZE));
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  // quick way to check
  if (bgd->bg_free_blocks_count < require_block) {
    res[0] = -1;
    return res;
  } else {
    int counter = 0;
    int index = 0;
    for (int i = 0; i < sb->s_blocks_count; i++) {
        unsigned c = bm[i / 8];                     // get the corresponding byte
        if ((c & (1 << index)) == 0) {
          res[counter] = i + 1;
          counter = counter + 1;
          if (counter == require_block) {
            return res;
          }
        }
        if (++index == 8) (index = 0); // increment shift index, if > 8 reset.
    }
    // if there it reach here, that means
    // there isn't enough space
    // set the first value to -1 to indicate error.
    res[0] = -1;
    return res;
  }
}



/*
This function create the new dir entry and
linked to the input_inode

  place_inode - directroy where you place the inode
  disk - the disk image
  dir_name - either file name or  the directory name
  input_inode - the new inode number you created
  block_type - entry type

  return 1 if out of block
  return 0 if everything is fine
*/
int add_link_to_dir(struct ext2_inode* place_inode,
  unsigned char* disk, char* dir_name, unsigned int input_inode,
  	unsigned char block_type) {
  // link between the dir_inode and the place_inode
  int block_num;
  int *in_direct_block;
  int inserted = 0;
  int i;
  int block_usage = 0;
  struct ext2_dir_entry_2 *new_dir;
  unsigned long pos;
  // loop throgh the place inode's blocks
  for (i = 0; i < place_inode->i_blocks; ++i) {
      if (i < 12) {
          block_num = place_inode->i_block[i];
      } else {
          in_direct_block = (int *) (disk + EXT2_BLOCK_SIZE * place_inode->i_block[12]);
          block_num = in_direct_block[i - 12];
      }
      pos = (unsigned long) disk + block_num * EXT2_BLOCK_SIZE;
      struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) pos;
      inserted = 0;
      do {
          // Get the length of the current block and type
          int cur_len = dir->rec_len;
          // check if there is room to place a new entry here
          if (cur_len - get_min_rec_len(dir->name_len) >= get_min_rec_len(strlen(dir_name))) {
            // put it in and break the loop;
            // 1. decrease the res_length of current entry
            // printf("%s %d %d %d\n", dir->name, cur_len, get_min_rec_len(dir->name_len), get_min_rec_len(strlen(dir_name)) );
            dir->rec_len = cur_len - get_min_rec_len(strlen(dir_name));
            // 2. creat a new entry
            pos = pos + dir->rec_len;
            new_dir = (struct ext2_dir_entry_2 *) pos;
            // 3. fill in the data
            strcpy(new_dir->name, dir_name);
            new_dir->name_len = strlen(dir_name);
            new_dir->inode = input_inode;
            new_dir-> rec_len = get_min_rec_len(new_dir->name_len);
            new_dir-> file_type = block_type;
            inserted = 1;
            break;
          }
          pos = pos + cur_len;
          dir = (struct ext2_dir_entry_2 *) pos;
          // Last directory entry leads to the end of block. Check if
          // Position is multiple of block size, means we have reached the end
      } while (pos % EXT2_BLOCK_SIZE != 0);
      if (inserted == 1) {
        break;
      }
  }
  if (inserted != 1) {
    // create a new block and link it
    // 1. create a new blocks
    int* free_blocks = find_free_blocks(disk, 1);
    if (free_blocks[0] == -1) {
      return 1;
    }
    pos = (unsigned long) (disk + EXT2_BLOCK_SIZE * free_blocks[0]);
    new_dir = (struct ext2_dir_entry_2 *) pos;
    strcpy(new_dir->name, dir_name);
    new_dir->name_len = strlen(dir_name);
    new_dir->inode = input_inode;
    new_dir-> rec_len = get_min_rec_len(new_dir->name_len);
    new_dir-> file_type = block_type;
    // if the last block is at normal blcok
    if (i < 12) {
      place_inode->i_block[i] = free_blocks[0];
    } else if (i > 12) {
      // if there is already an in_direct_block
      in_direct_block = (int *) (disk + EXT2_BLOCK_SIZE * place_inode->i_block[12]);
      in_direct_block[i - 12] = free_blocks[0];
    } else {
      // if it is on the in_direct_block
      // need another block as indirec t
      int* free_indirect_blocks = find_free_blocks(disk, 1);
      if (free_indirect_blocks[0] == -1) {
        return 1;
      }
      in_direct_block = (int *)  (disk + EXT2_BLOCK_SIZE * free_indirect_blocks[0]);
      in_direct_block[0] =  free_blocks[0];
      place_inode->i_block[12] = in_direct_block[0];
      set_bitmap(1, disk, free_indirect_blocks[0], 1);
      block_usage = block_usage + 1;
    }
    set_bitmap(1, disk, free_blocks[0], 1);
    // once the allocate and operation is truely completed
    block_usage = block_usage + 1;
    struct   ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
    bgd->bg_free_blocks_count -= block_usage;
    // also update the block usage
  }
  return 0;
}
