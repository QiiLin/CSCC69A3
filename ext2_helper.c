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


/*
Note if the last is / we need to handle it specially
Note: the passed in str will be modified
*/
char* parse_path(char* target_path) {
  char* token = strtok(target_path, "/");
  return token;
}

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
    fprintf(stdout, "read path %s\n", name );
    FILE *fl = fopen(name, "r");
    fseek(fl, 0, SEEK_END);
    long len = ftell(fl);
    char *ret = (char *) malloc(len);
    fseek(fl, 0, SEEK_SET);
    fread(ret, 1, len, fl);
    fclose(fl);
    return ret;
}

// int check_blocks(unsigned char *disk, int file_size) {
//     int needed_blocks = file_size / 1024;
//     if (file_size % 1024 != 0) {
//         needed_blocks++;
//     }
//     if (needed_blocks <= num_free_blocks(disk)) {
//         return 1;
//     }
//     return 0;
// }

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
Note: the arg_path will stay the same
**/
int read_path(unsigned char* disk, char* arg_path) {
  // go through the path without change the input
  char* copy_path =(char*) calloc(strlen(arg_path)+1, sizeof(char));
  strcpy(copy_path, arg_path);
  char* path = parse_path(copy_path);

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
          free(copy_path);
          return -1;
        }
        current = strtok(NULL, "/");
        // printf("%d %s %s  ddss\n", current_inode_index, current, temp );
      }
      // it is look for
      if (type != 'd') {
        // error checking: file/dir is invalid
        if (current != NULL) {
          free(copy_path);
          return -1;
        }
      }
  }
  free(copy_path);
  return current_inode_index;
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
set the bit map value by the input value
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

/**
return a inode number of there is inode
return -1 if there isn't such inode exist
**/
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
    for (int i = 0; i < bgd->bg_free_blocks_count; i++) {
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
  printf("%s and %d \n",  dir_name, input_inode);
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
      printf("%s and block %d \n",  dir_name, block_num);
      do {
          // Get the length of the current block and type
          int cur_len = dir->rec_len;
          // check if there is room to place a new entry here
          if (cur_len - get_min_rec_len(dir->name_len) >= get_min_rec_len(strlen(dir_name))) {
            printf("prev need size %d and need size %d \n",  get_min_rec_len(dir->name_len), get_min_rec_len(strlen(dir_name)));
            // put it in and break the loop;
            // 1. decrease the res_length of current entry
            // printf("%s %d %d %d\n", dir->name, cur_len, get_min_rec_len(dir->name_len), get_min_rec_len(strlen(dir_name)) );
            dir->rec_len = get_min_rec_len(dir->name_len);
            // 2. creat a new entry
            pos = pos + dir->rec_len;
            new_dir = (struct ext2_dir_entry_2 *) pos;
            // 3. fill in the data
            strcpy(new_dir->name, dir_name);
            new_dir->name_len = strlen(dir_name);
            new_dir->inode = input_inode;
            new_dir-> rec_len = cur_len - dir->rec_len;
            new_dir-> file_type = block_type;
            printf("prev need size %d and need size %d \n",  dir->rec_len, new_dir-> rec_len);
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
    struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
    bgd->bg_free_blocks_count -= block_usage;
    // also update the block usage
  }
  return 0;
}

/**
This function takes a path
and make change the path so that
the last file/directory entry is being store
and the path is terminated at parent of the
last file/Directory

Note: The file_path will be modified
Note: dir_name will be modified
**/
void pop_last_file_name(char *file_path, char* dir_name) {
   // loop through the path and remove the last directory in it
   // and store it somewhere,
   int i;
   int prev = -1;
   int path_length = strlen(file_path);
   int counter = 0;
   for (i = 0; i < path_length; i++) {
     // if the current is sperator
     if (file_path[i] == '/') {
       prev = i;
       // reset dir_name
       dir_name[0] = '\0';
       counter = 0;
     } else {
       //keep storing dir_name;
       dir_name[counter] = file_path[i];
       counter = counter + 1;
       dir_name[counter] = '\0';
     }
   }
   // note there has to be at least on / so prev is always not -1
   file_path[prev] = '\0';
   printf("%s\n",  dir_name);
}


struct ext2_inode * initialize_inode(unsigned char* disk, int inode_num, unsigned short type, int size ){
  struct ext2_inode *new_inode = get_inode(disk, inode_num);
  new_inode->i_mode = type;
  new_inode->i_size = size;
  new_inode->i_links_count = 1;
  new_inode->i_blocks = 0;
  return new_inode;
}

/**
this return the sub string from i to j - 1
**/
void substr(char * path, int i , int j, char* res) {
  for ( int k = i; k < j; j++) {
    res[k - i] = path[k];
  }
  res[j - i] = '\0';
}

/**
This checks if the path is a directory
return -1 if it is not found or is not a directory
return >= 0 if inode is found
**/
// int check_valid_path(unsigned char *disk, char *path) {
//     // step 1: make copy of path and read the path
//     char* tempstr =(char*) calloc(strlen(path)+1, sizeof(char));
//     strcpy(tempstr, path);
//     char* passed_path = parse_path(tempstr);
//     int inodenum = read_path(disk, passed_path);
//     free(tempstr);
//     // step 2: check if it is a valid found
//     if (inodenum == -1) {
//       fprintf(stderr, "%s: No such directory\n", path);
//       exit(ENOENT);
//     }
//     struct ext2_inode *place_inode = get_inode(disk, inodenum);
//     // step 3 check if it is a valid
//     if (!S_ISDIR(place_inode->i_mode)) {
//       fprintf(stderr, "%s: No such directory\n", path);
//       exit(ENOENT);
//     }
//     return inodenum;
// }


// given file name and its directory inodenumber, check if file_name is within the direcotry already
// return 1 if it does not exist, -1 o/w.
int check_valid_file(unsigned char *disk, int dir_inodenum, char *file_name) {
    printf("\n\n\n");
    printf("file name to check is %s\n", file_name);
    struct ext2_inode* dir_inode = get_inode(disk, dir_inodenum);
    int blocknum;
    for (int i = 0; i < dir_inode->i_blocks; i++ ) {
      if (i < 12) {
        // direct case
        blocknum = dir_inode->i_block[i];
      } else {
        // indirect case
        int *in_dir = (int *) (disk + EXT2_BLOCK_SIZE * dir_inode->i_block[12]);
        blocknum = in_dir[i - 12];
      }
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
     }
    return 1;
}
