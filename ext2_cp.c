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
    // get file byte size
    int file_size = fileInfo.st_size;
    char *file_name = get_file_name(argv[2]);
    char *file_contents = readFileBytes(argv[2]);
    printf("file size is %d\n", file_size);
    printf("file name is %s, the length of file name is %lu\n", file_name, strlen(file_name));
    printf("file contents is %s\n", file_contents);
    printf("length of contents is %lu\n", strlen(file_contents));
    int valid_filename = check_valid_file(disk, dir_inode, file_name);
    if (valid_filename < 0) {
        fprintf(stderr, "A file named %s already exists in the directory",file_name);
        exit(EEXIST);
    }
    int free_blocks = num_free_blocks(disk);
    int free_inodes = num_free_inodes(disk);
    printf("    free blocks: %d\n", free_blocks);
    printf("    free inodes: %d\n", free_inodes);
    // allocate new inode for the file
    // int allocated_block = allocate_block(disk);
    // printf("allocated blocknum is %d\n", allocated_blocknum);
    int allocated_inode = allocate_inode(disk, file_size, file_name);
    if (allocated_inode > 0) {
        int dirent = allocate_dirent(disk, file_name, allocated_inode, dir_inode);
        printf("finished");
    } else {
        // there were not enough inode or block space
        perror("not enough space for inode or blocks for the file");
    }
    return 0;

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
    FILE *fl = fopen(name, "r");  
    fseek(fl, 0, SEEK_END);  
    long len = ftell(fl);  
    char *ret = malloc(len);  
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
            char type = (dir->file_type == EXT2_FT_REG_FILE) ? 'f' :
                        ((dir->file_type == EXT2_FT_DIR) ? 'd' : 's');
            char *name = dir->name;
            printf("directory name is %s\n", name);
            //printf("directory type is %s\n", type);
            printf("directory inode is %d\n", dir->inode);
            if (type == 'f' && (strcmp(name, file_name) == 0)) {
                printf("found a file with file_name\n");
                // found the corresponding directory
                // set inodenum to this directory's inode
                // set found_inode to be 1 to break out of the current while loop
                // to move on to the next inode dirent
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