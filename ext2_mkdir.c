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
  int * res = malloc(sizeof(int) * require_block) ;
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


int main(int argc, char **argv) {
  unsigned char *disk;
  char* current_path;
  char * passed_path;
  if (argc != 3 || argv[2][0] != '/') {
    fprintf(stderr, "Usage: %s disk_img path\n", argv[0]);
    exit(1);
  }
  current_path = argv[2];
  disk = read_disk(argv[1]);
  char* tempstr = calloc(strlen(current_path)+1, sizeof(char));
  strcpy(tempstr, current_path);
  passed_path = parse_path(tempstr);
  int inode_index = read_path (disk, passed_path);
  free(tempstr);
  if (inode_index != -1) {
    fprintf(stderr, "%s: already exist\n", argv[2]);
    exit(EEXIST);
  }
  // loop through the path and remove the last directory in it
  // and store it somewhere,
  int i;
  int prev = -1;
  int path_length = strlen(current_path);
  char dir_name[path_length];
  int counter = 0;
  for (i = 0; i < path_length; i++) {
    // if the current is sperator
    if (current_path[i] == '/') {
      prev = i;
      // reset dir_name
      dir_name[0] = '\0';
      counter = 0;
    } else {
      //keep storing dir_name;
      dir_name[counter] = current_path[i];
      counter = counter + 1;
      dir_name[counter] = '\0';
    }
  }
  // note there has to be at least on / so prev is always not -1
  current_path[prev] = '\0';
  // printf("%s %s222\n", current_path, dir_name);
  // get the node number
  inode_index = read_path(disk, parse_path(current_path));
  // check if exist
  if (inode_index == -1) {
    fprintf(stderr, "%s: No such directory\n", argv[2]);
    exit(ENOENT);
  }
  struct ext2_inode *place_inode = get_inode(disk, inode_index);
  // check if it is a directory
  if (!S_ISDIR(place_inode->i_mode)) {
      fprintf(stderr, "%s: No such directory\n", argv[2]);
      exit(ENOENT);
  }

  // check if we have enough space for it
  int free_inode_index = find_free_inode(disk);
  if (free_inode_index == -1) {
    fprintf(stderr, "%s: no inode avaiable\n", argv[0]);
    exit(1);
  }
  int* free_blocks = find_free_blocks(disk, 1);
  if (free_blocks[0] == -1) {
    fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
    exit(1);
  }
  // printf("%d %d \n", free_blocks[0], free_inode_index);
  // update bitmap to make sure we are working on it for sure
  set_bitmap(0, disk, free_inode_index, 1);
  set_bitmap(1, disk, free_blocks[0], 1);
  // do the linkage and check if there is enough space
  int result = add_link_to_dir(place_inode, disk, dir_name, free_inode_index,
      EXT2_FT_DIR);
  if (result == -1) {
    // need to revert the bitmap
    set_bitmap(0, disk, free_inode_index, 0);
    set_bitmap(1, disk, free_blocks[0], 0);
    fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
    exit(1);
  }
  // init inode amd set up property
  struct ext2_inode *dir_inode = get_inode(disk, free_inode_index);
  dir_inode->i_mode = EXT2_S_IFDIR;
  dir_inode->i_size = EXT2_BLOCK_SIZE;
  dir_inode->i_links_count = 1;
  dir_inode->i_blocks = 0;
  // init block as dir_entry for the dir_inode
  // TODO need to refactor this into a function
  unsigned long pos = (unsigned long) disk + EXT2_BLOCK_SIZE * free_blocks[0];
  struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) pos;
  dir_entry->name[0] = '.';
  dir_entry->name_len = 1;
  dir_entry->inode = free_inode_index;
  dir_entry->rec_len = 12;
  dir_entry->file_type = EXT2_FT_DIR;

  pos = pos + dir_entry->rec_len;
  dir_entry = (struct ext2_dir_entry_2 *) pos;

  dir_entry->name[0] = '.';
  dir_entry->name[1] = '.';
  dir_entry->name_len = 2;
  dir_entry->inode = inode_index;
  dir_entry->rec_len = EXT2_BLOCK_SIZE - 12;
  dir_entry->file_type = EXT2_FT_DIR;
  dir_inode->i_links_count ++;
  dir_inode->i_block[0] = free_blocks[0];
  dir_inode->i_blocks = 1;
  // udpate the group descriptor
  struct   ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  bgd->bg_free_blocks_count -= 1;
  bgd->bg_free_inodes_count -= 1;
}
