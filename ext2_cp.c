#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

int blocks_needed(long int file_size);
int check_valid_path(unsigned char *disk, char *path);
struct ext2_inode *get_inode (unsigned char *disk, int inodenum);
int allocate_blocks(unsigned char *disk);
int allocate_inode(unsigned char *disk);

unsigned char *disk;

int main(int argc, char **argv) {
    if(argc != 4) {
        fprintf(stderr, "Usage: readimg <ext2 image file name> <file path on your OS> <abs file path on image>\n");
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);
    int fd2 = open(argv[2], O_RDONLY);
    if (fd2 < 0) {
        return -ENOENT;
    }
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	    perror("mmap");
	    exit(1);
    }
    struct stat fileInfo;
    if (fstat(fd2, &fileInfo) != 0) {
        perror("file was not found");
        exit(ENOENT);
    }
    if (!S_ISREG(fileInfo.st_mode)) {
        perror("wrong file type given");
        exit(1);
    }
    // check if the path passed in is a valid asbolute path leading to a directory
    // int is_path = check_valid_path(inum, inumc, dirs, dirsin, sb, bmi, bgd, in, argv[3]);
    
    int dir_inode = check_valid_path(disk, argv[3]);
    printf("is_path variable value is %d\n", dir_inode);
    if (dir_inode < 0) {
        fprintf(stderr, "No such file or directory\n");
        exit(ENOENT);
    }
    int free_blocks = num_free_blocks(disk);
    int free_inodes = num_free_inodes(disk);
    printf("    free blocks: %d\n", free_blocks);
    printf("    free inodes: %d\n", free_inodes);
    // get file byte size
    int file_size = fileInfo.st_size;
    printf("file size is %d\n", file_size);
    // number of needed block
    int needed_blocks = blocks_needed(file_size);
    printf("needed blocks is %d\n", needed_blocks);
    if (free_blocks < needed_blocks || free_inodes == 0) {
        perror("there are not enough blocks or inodes for the new file\n");
        exit(-1);
    }
    printf("there are enough space\n");
    // allocate new inode for the file
    int allocated_inode = allocate_inode(disk);
    return 0;

}

struct ext2_inode *get_inode (unsigned char *disk, int inodenum) {
  // stuff from tut exercise
  // 1. get group descriptor
  struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  // 2. get inode_table
  struct ext2_inode * inode_table = (struct ext2_inode *) (disk + bgd->bg_inode_table * EXT2_BLOCK_SIZE);
  // 3. find the target inode, Note -1 is need here it is start at one by
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

int allocate_blocks(unsigned char *disk) {

}

int allocate_inode(unsigned char *disk) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // get the inode bitmap to manipulate its bits if there are free inode in the bitmap
    struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
    char *bmi = (char *) (disk + (bgd->bg_inode_bitmap * EXT2_BLOCK_SIZE));
    int index2 = 0;

    if (sb->s_free_inodes_count > 0) {
        for (int i = 0; i < sb->s_inodes_count; i++) {
        unsigned c = bmi[i / 8];                     // get the corresponding byte
        int not_used = c & (1 << index2) == 0;
        if (not_used) {
            // found a free inode in the bitmap
            // try to allocate data blocks for this inode
            // if successful, decrease super block free inode count
            allocate_blocks(disk);
        }
        printf("%d", (c & (1 << index2)) == 0);       // Print the correcponding bit
        // If that bit was a 1, inode is used, store it into the array.
        // Note, this is the index number, NOT the inode number
        // inode number = index number + 1
        if (((c & (1 << index2)) > 0) == 0 && i > 10) {    // > 10 because first 11 not used

        }
        if (++index2 == 8) (index2 = 0, printf(" ")); // increment shift index, if > 8 reset.
    };
    } else {
        perror("Theres no enough inode space");
        return -1;
    }
}

int blocks_needed(long int file_size) {
    return file_size % EXT2_BLOCK_SIZE == 0 ? file_size / EXT2_BLOCK_SIZE : file_size / EXT2_BLOCK_SIZE + 1;
}