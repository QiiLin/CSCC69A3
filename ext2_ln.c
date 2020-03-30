#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include "ext2_cp.c"
#include "ext2_ls.c"

int get_dir_inodenum(unsigned char *disk, char *file_name);

unsigned char *disk;

int main(int argc, char **argv) {
    int soft_link = 0;
    char *disk_file;
    char *source_file;
    char *target_file;
    // there must be at least 3 command line arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: ext2_ln <ext2 image file name> [-a] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
        exit(1);
    }
    // no -a option, check if there is one
    if (argc == 4) {
        int opt;
        while ((opt = getopt(argc, argv, "a:")) != -1) {
            switch (opt)
            {
            case 'a':
                fprintf(stderr, "Usage: ext2_ln <ext2 image file name> [-a] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
                exit(1);
            default:
                abort();
            }
        }
        source_file = argv[2];
        target_file = argv[3];
    // has -a option,
    } else if (argc == 5){
        int opt;
        int option_index;
        for (int i = 1; i < argc; i++) {
            char *arg = argv[i];
            if (strcmp(arg, "-a") == 0) {
                if (i == 1) {
                   fprintf(stderr, "Usage: ext2_ln <ext2 image file name> [-a] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
                   exit(1);
                }
                option_index = i;
                soft_link = 1;
            }
        }
        if (soft_link == 0) {
            fprintf(stderr, "Usage: ext2_ln <ext2 image file name> [-a] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
            exit(1);
        }
        if (option_index == 2) {
            source_file = argv[3];
            target_file = argv[4];
        } else if (option_index == 3) {
            source_file = argv[2];
            target_file = argv[4];
        } else {
            source_file = argv[2];
            target_file = argv[3];
        }
    } else {
        fprintf(stderr, "Usage: ext2_ln <ext2 image file name> [-a] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
        exit(1);
    }
    disk_file = argv[1];
    int fd = open(disk_file, O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
	    perror("mmap");
	    exit(1);
    }
    // check source file path validity
    char* tmp_source_file = calloc(strlen(source_file)+1, sizeof(char));
    strcpy(tmp_source_file, source_file);
    char *passed_source_file = parse_path(source_file);
    int source_file_inodenum = read_path(disk, passed_source_file);
    // if source file does not exist
    if (source_file_inodenum < 0) {
        perror("Source file does not exist.");
        exit(ENOENT);
    }
    struct ext2_inode* source_file_inode = get_inode(disk, source_file_inodenum);
    // check if source file is of type file
    // If source file is a directory, exit
    if (S_ISDIR(source_file_inode->i_mode)) {
        perror("Source file path was given as a directiory, file is required.");
        exit(EISDIR);
    }
    // check target file path validity
    char *tmp_target_file = calloc(strlen(target_file)+1, sizeof(char));
    strcpy(tmp_target_file, target_file);
    printf("first tmp copy is :%s\n", tmp_target_file);
    char *second_copy_tmp_target_file = calloc(strlen(target_file)+1, sizeof(char));
    strcpy(second_copy_tmp_target_file, target_file);
    printf("second copy is :%s\n", second_copy_tmp_target_file);
    char *passed_target_file = parse_path(target_file);
    printf("after parse target file name is :%s\n", target_file);
    printf("after parse first copy is :%s\n", tmp_target_file);
    printf("after parse second copy is :%s\n", second_copy_tmp_target_file);
    int target_file_inodenum = read_path(disk, passed_target_file);
    if (target_file_inodenum > 0) {
        struct ext2_inode* target_file_inode = get_inode(disk, target_file_inodenum);
        if (S_ISREG(target_file_inode->i_mode)) {
            perror("Target file was given as a directory");
            exit(EISDIR);
        }
        perror("Target file already existed, please give another path");
        exit(EEXIST);
    }
    char *target_filename = get_file_name(tmp_target_file);
    printf("after target file name first copy is :%s\n", tmp_target_file);
    int target_dir_inodenum = 0;
    // if it is a hardlink, copy a dir_ent only and increase source inode link count
    if (!soft_link) {
        source_file_inode->i_links_count++;
        int target_dir_inodenum = get_dir_inodenum(disk, second_copy_tmp_target_file);
        printf("the directory inode num is : %d\n", target_dir_inodenum);
        // get target file's directory inode number and get the source file's dir_entry information
        //int dirent = allocate_dirent(disk, target_filename, source_file_inodenum, target_dir_inodenum);
    } else {

    }
    // if it is a softlink, create a new inode where the data is the file path to the source file
    
}

int get_dir_inodenum(unsigned char *disk, char *path) {
    printf("path passed in is %s\n", path);
    int inodenum = EXT2_ROOT_INO;
    char *splitted_path;
    int found_inode = 0;
    splitted_path = strtok(path, "/");
    while (splitted_path != NULL && found_inode == 0) {
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
            // if pos is a multiple of block size and found_inode is 0 then the inodenum would be the
            // parent dir inodenum
            if (found_inode == 0) {
                return inodenum;
            }
            *arr++;
        }
        splitted_path = strtok(NULL, "/");
    }
    return inodenum;
}