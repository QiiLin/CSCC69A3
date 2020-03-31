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
#include "ext2_helper.c"

int get_dir_inodenum(unsigned char *disk, char *file_name);

unsigned char *disk;

int main(int argc, char **argv) {
    // TODO to add check of abs after it is working!
    int soft_link = 0;
    char *disk_file;
    char *source_file;
    char *target_file;
    // there must be at least 3 command line arguments
    if (argc < 4) {
        fprintf(stderr, "1Usage: ext2_ln <ext2 image file name> [-s] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
        exit(1);
    }
    // no -a option, check if there is one
    if (argc == 4) {
        int opt;
        while ((opt = getopt(argc, argv, "s:")) != -1) {
            switch (opt)
            {
            case 's':
                fprintf(stderr, "2Usage: ext2_ln <ext2 image file name> [-s] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
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
            if (strcmp(arg, "-s") == 0) {
                if (i == 1) {
                   fprintf(stderr, "3Usage: ext2_ln <ext2 image file name> [-s] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
                   exit(1);
                }
                option_index = i;
                soft_link = 1;
            }
        }
        if (soft_link == 0) {
            fprintf(stderr, "4Usage: ext2_ln <ext2 image file name> [-s] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
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
        fprintf(stderr, "5Usage: ext2_ln <ext2 image file name> [-s] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
        exit(1);
    }

    if (source_file[0] != '/' || target_file[0] != '/') {
      fprintf(stderr, "6Usage: ext2_ln <ext2 image file name> [-s] <absolute source file path on disk image> <absolute path target file location on disk image>\n");
      exit(1);
    }
    disk = read_disk(argv[1]);
    // check source file path validity
    char* tmp_source_file = (char *)calloc(strlen(source_file)+1, sizeof(char));
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
    char *tmp_target_file = (char *) calloc(strlen(target_file)+1, sizeof(char));
    strcpy(tmp_target_file, target_file);
    printf("first tmp copy is :%s\n", tmp_target_file);
    char *second_copy_tmp_target_file = (char *) calloc(strlen(target_file)+1, sizeof(char));
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
    int name_len = strlen(second_copy_tmp_target_file);
    char target_filename[name_len];
    get_file_name_temp(tmp_target_file,target_filename);
    printf("after target file name first copy is :%s\n", tmp_target_file);
    // get the target directory number
    int target_dir_inodenum = read_path(disk, parse_path(tmp_target_file));
    if (target_dir_inodenum == -1) {
      perror("Target 2directory doesn't exist");
      exit(ENOENT);
    }
    struct ext2_inode* dest_dir_inode = get_inode(disk, target_dir_inodenum);
    // if it is not a directroy
    if (!S_ISDIR(dest_dir_inode->i_mode)) {
        perror("Target3 directory can not be a file");
        exit(ENOENT);
    }
    // if it is a hardlink, copy a dir_ent only and increase source inode link count
    if (!soft_link) {
        source_file_inode->i_links_count++;
        printf("the directory inode num is : %d %s\n", target_dir_inodenum,target_filename);
        int result = add_link_to_dir
        (dest_dir_inode, disk, target_filename, source_file_inodenum,
            EXT2_FT_REG_FILE);
        if (result == -1) {
          fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
          exit(1);
        }
        // get target file's directory inode number and get the source file's dir_entry information
    } else {
      // // check if the name is more than the max length
      // if (name_len > EXT2_NAME_LEN) {
      //   perror("The target filename is too long");
      //   exit(1);
      // }
      int free_inode_index = find_free_inode(disk);
      if (free_inode_index == -1) {
        fprintf(stderr, "%s: no inode avaiable\n", argv[0]);
        exit(1);
      }
          printf("path passed in is %d\n", free_inode_index);
      struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
      struct ext2_inode * new_inode = initialize_inode(disk, free_inode_index, EXT2_S_IFLNK, name_len*4);
      if (name_len <= 15) {
        // TODO need to double check with aht is the i-blocks value
        new_inode->i_blocks = 1;
        // set the path into the path variable
        for (int i = 0; i < name_len; i++) {
          new_inode->i_block[i] = (unsigned int) second_copy_tmp_target_file[i];
        }
        printf("path passed in wis %s\n", (unsigned char*) new_inode->i_block);
        // update bit map for inode
        set_bitmap(0, disk, free_inode_index, 1);
        // update the dest directory with a new dir_entry
        int result = add_link_to_dir(dest_dir_inode, disk, target_filename, free_inode_index,
            EXT2_FT_SYMLINK);
        if (result == -1) {
          // need to revert the bitmap
          set_bitmap(0, disk, free_inode_index, 0);
          fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
          exit(1);
        }
        // remember to update the i_links_count
        // dest_dir_inode->i_links_count++;
        // udpate the group descriptor
        bgd->bg_free_inodes_count -= 1;
      } else {
        // count how many block we needed here
        int required_block = ((name_len*4) / EXT2_BLOCK_SIZE) + 1;
        printf("required_block %d %d\n", name_len, required_block);
        // get the free blocks
        int* free_blocks = find_free_blocks(disk, required_block);
        for (int i = 0; i < required_block; i++) {
          if (free_blocks[i] == -1) {
            fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
            exit(1);
          }
        }
        // claim those block
        for (int i = 0; i < required_block; i++) {
          set_bitmap(0, disk, free_blocks[i], 1);
        }
        // claim those inode
        set_bitmap(0, disk, free_inode_index, 1);
        // create the linkage
        int result = add_link_to_dir(dest_dir_inode, disk, target_filename, free_inode_index,
            EXT2_FT_SYMLINK);
        if (result == -1) {
          // need to revert the bitmap
          for (int i = 0; i < required_block; i++) {
            set_bitmap(0, disk, free_blocks[i], 0);
          }
          set_bitmap(0, disk, free_inode_index, 0);
          fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
          exit(1);
        }
        // start fill the data
        new_inode->i_blocks = required_block;
        new_inode->i_size = required_block*EXT2_BLOCK_SIZE;
        // fills the path in it
        // NOTE: this doesn't handle the case where the path length*4 is bigger than
        // 1024*12
        int start = 0;
        int end = 0;
        for (int i = 0; i < required_block; i++) {
          // get the start of block
          unsigned char * current_block = (unsigned char *) disk + EXT2_BLOCK_SIZE * free_blocks[i];
          if (i == required_block-1) {
            start = i*EXT2_BLOCK_SIZE;
            end = start + (name_len*4) % EXT2_BLOCK_SIZE;
          } else {
            start =  i*EXT2_BLOCK_SIZE;
            end = (i+1) *EXT2_BLOCK_SIZE;
          }
          memcpy(current_block, substr(second_copy_tmp_target_file, start, end), end - start);
        }
        // udpate the group descriptor
        bgd->bg_free_inodes_count -= 1;
        bgd->bg_free_blocks_count -= required_block;
      }
    }
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
