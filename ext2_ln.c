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

    if (source_file[strlen(source_file) - 1] == '/') {
      fprintf(stderr, "ln: failed to access %s: Not allow directory\n", source_file);
      exit(ENOENT);
    }
    if (target_file[strlen(target_file) - 1] == '/') {
      fprintf(stderr, "ln: failed to access %s: Not allow directory\n", target_file);
      exit(ENOENT);
    }
    disk = read_disk(argv[1]);
    // check source file path validity
    int source_file_inodenum = read_path(disk, source_file);
    // if source file does not exist
    if (source_file_inodenum < 0) {
        fprintf(stderr, "Source file does not exist\n");
        exit(ENOENT);
    }
    struct ext2_inode* source_file_inode = get_inode(disk, source_file_inodenum);
    // check if source file is of type file
    // If source file is a directory, exit
    if (S_ISDIR(source_file_inode->i_mode)) {
        fprintf(stderr, "Source file path was given as %d a directiory, file is required.\n",  source_file_inodenum);
        exit(EISDIR);
    }
    // check target file path validity
    int target_file_inodenum = read_path(disk, target_file);
    if (target_file_inodenum > 0) {
        struct ext2_inode* target_file_inode = get_inode(disk, target_file_inodenum);
        if (S_ISREG(target_file_inode->i_mode)) {
            perror("Target file was given as a directory");
            exit(EISDIR);
        }
        perror("Target file already existed, please give another path");
        exit(EEXIST);
    }
    int name_len = strlen(source_file);
    char target_filename[name_len];

    char* copy_path =(char*) calloc(strlen(target_file)+1, sizeof(char));
    strcpy(copy_path, target_file);
    pop_last_file_name(copy_path, target_filename);
    // get the target directory number
    int target_dir_inodenum = read_path(disk, copy_path);
    free(copy_path);
    if (target_dir_inodenum == -1) {
      perror("Target directory doesn't exist");
      exit(ENOENT);
    }
    struct ext2_inode* dest_dir_inode = get_inode(disk, target_dir_inodenum);
    // if it is not a directroy
    if (!S_ISDIR(dest_dir_inode->i_mode)) {
        perror("Target parent directory can not be a file");
        exit(ENOENT);
    }
    // if it is a hardlink, copy a dir_ent only and increase source inode link count
    if (!soft_link) {
        source_file_inode->i_links_count++;
        int result = add_link_to_dir
        (dest_dir_inode, disk, target_filename, source_file_inodenum,
            EXT2_FT_REG_FILE);
        if (result == -1) {
          fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
          exit(1);
        }
        // get target file's directory inode number and get the source file's dir_entry information
    } else {
      int free_inode_index = find_free_inode(disk);
      if (free_inode_index == -1) {
        fprintf(stderr, "%s: no inode avaiable\n", argv[0]);
        exit(1);
      }
      struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
      struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
      struct ext2_inode * new_inode = initialize_inode(disk, free_inode_index, EXT2_S_IFLNK, name_len);
      if (name_len <= 15) {
        // TODO need to double check with aht is the i-blocks value
        new_inode->i_blocks = 0;
        // set the path into the path variable
        // for (int i = 0; i < name_len; i++) {
        //   new_inode->i_block[i] = (unsigned int) source_file[i];
        // }
        memcpy(new_inode->i_block, source_file, name_len);
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
        sb->s_free_inodes_count -= 1;
      } else {
        // count how many block we needed here
        int required_block = get_required_block(name_len);
        int indir_block = 0;
        // check if it need redirect block
        if (required_block > 12) {
          indir_block = 1;
        }
        // get the free blocks
        int* free_blocks = find_free_blocks(disk, required_block + indir_block);
        if (free_blocks[0] == -1) {
          fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
          exit(1);
        }
        // claim those block
        for (int i = 0; i < required_block + indir_block; i++) {
          set_bitmap(1, disk, free_blocks[i], 1);
        }
        // claim those inode
        set_bitmap(0, disk, free_inode_index, 1);
        // create the linkage
        int result = add_link_to_dir(dest_dir_inode, disk, target_filename, free_inode_index,
            EXT2_FT_SYMLINK);
        if (result == -1) {
          // need to revert the bitmap
          for (int i = 0; i < required_block + indir_block; i++) {
            set_bitmap(1, disk, free_blocks[i], 0);
          }
          set_bitmap(0, disk, free_inode_index, 0);
          fprintf(stderr, "%s: no blocks avaiable\n", argv[0]);
          exit(1);
        }
        // start fill the
        new_inode->i_blocks = (required_block + indir_block) *2;
        new_inode->i_size = name_len;
        unsigned int * indirect_block;
        // start set up blocks
        for(int i = 0; i < required_block; i++) {
          if (i < 12) {
            new_inode->i_block[i] = free_blocks[i];
          } else {
            // if it reach here then the indir_block must equal to 1
            // so the  free_blocks[required_block]; exist
            if (i == 12 ) {
              // first time access inderect so we need init the indirect_block
              new_inode->i_block[12] = free_blocks[required_block];
              indirect_block = (unsigned int *)(disk + new_inode->i_block[12] * EXT2_BLOCK_SIZE);
            }
            indirect_block[i-12] = free_blocks[i];
          }
        }
        int start = 0;
        int end = 0;
        int block_number;
        int i;
        char current[EXT2_BLOCK_SIZE + 1];
        for ( i = 0; i < required_block; i++) {
          if (i < 12) {
            // direct case
            block_number = new_inode->i_block[i];
          } else {
            // indirect case
            unsigned int *in_dir = (unsigned int *) (disk + EXT2_BLOCK_SIZE * new_inode->i_block[12]);
            block_number = in_dir[i - 12];
          }

          int padding = 0;
          // get the start of block
          unsigned char * current_block = (unsigned char *) disk + EXT2_BLOCK_SIZE * block_number;
          if (i == required_block - 1) {
            start = i*EXT2_BLOCK_SIZE;
            end = start + (name_len) % EXT2_BLOCK_SIZE;
            padding += 1;
          } else {
            start =  i*EXT2_BLOCK_SIZE;
            end = (i+1) *EXT2_BLOCK_SIZE;
          }
          // strncpy(current, source_file + start, end);
          substr(source_file, start, end, current);
          memcpy(current_block, current, EXT2_BLOCK_SIZE);
        }
        // udpate the group descriptor
        bgd->bg_free_inodes_count -= 1;
        sb->s_free_inodes_count -= 1;
        bgd->bg_free_blocks_count -= (required_block + indir_block);
        sb->s_free_blocks_count -= (required_block + indir_block);
      }
    }
}
