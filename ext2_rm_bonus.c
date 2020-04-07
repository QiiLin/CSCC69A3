#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ext2.h"
#include "ext2_helper.c"

/* Helper function for invalid input
*/
void show_usuage(char *proginput) {
    fprintf(stderr, "This command's Usage: %s disk_img [-r] abs_path\n", proginput);
}

int main(int argc, char **argv) {
    int is_dir_flag = 0;
    unsigned char *disk;
    char* image_path;
    char* delete_path;
    int r_flag = 0;
    // Check whether the input has exactly 3 input
    // if there are less than 3 argvs
    // If not return the error
    if (argc < 3 || argc > 4) {
        show_usuage(argv[0]);
        exit(1);
    }

    if (argc == 3) {
      image_path = argv[1];
      delete_path = argv[2];
      // there is -r tag
    } else {
      r_flag = 1;
      image_path = argv[1];
      if (strcmp("-r", argv[2]) == 0) {
        delete_path = argv[3];
      } else if (strcmp("-r", argv[3]) == 0) {
        delete_path = argv[4];
      } else {
        show_usuage(argv[0]);
        exit(1);
      }
    }

    // Check whether the destination path is a absolute path or not
    // If not, then terminate and show error
    if (delete_path[0] != '/') {
        show_usuage(argv[0]);
        exit(1);
    }
    // looking for directory
    if (delete_path[strlen(delete_path) - 1] == '/') {
      is_dir_flag = 1;
    }
    if (delete_path[strlen(delete_path) - 1] == '.') {
      fprintf(stderr, "%s: refuse to handle . or .. for the path: %s\n", argv[0], delete_path);
      exit(1);
    }
    // if the -r is not enable and it is looking for directory
    if (r_flag == 0 && is_dir_flag == 1) {
      fprintf(stderr, "%s: failed to access %s: Not allow directory\n", argv[0], delete_path);
      exit(1);
    }

    // Read the image first
    disk = read_disk(image_path);

    // Parse the path and find inode number
    // Get the innode number
    int inode_num = read_path(disk, delete_path);

    // if the inode number is -1
    // Then the directory can not be found
    if (inode_num == -1) {
        fprintf(stderr, "%s: No such fil2e or directory\n", argv[0]);
        exit(ENOENT);
    }

    // get the inode
    struct ext2_inode *tem_inode = get_inode(disk, inode_num);

    // if r-flag is not enable, but given  directo
    if (r_flag == 0 && S_ISDIR(tem_inode->i_mode)) {
        fprintf(stderr, "%s: cannot remove %s: Is a directory\n", argv[0], argv[2]);
        exit(EISDIR);
    }
    // if it is looing for directory but found non directory
    if(is_dir_flag == 1 && !S_ISDIR(tem_inode->i_mode)) {
      fprintf(stderr, "%s: failed to access %s: Not such directory\n", argv[0], delete_path);
      exit(ENOENT);
    }
    // // if the tem is dir and r_flag up.. remove
    // if (r_flag == 1 && S_ISDIR(tem_inode->i_mode) && delete_path[strlen(delete_path) - 1] == '/') {
    //   delete_path[strlen(delete_path) - 1]  = '\0';
    // }

    // initialize a dir name
    char file_name[EXT2_NAME_LEN + 1];

    // cut the the repository so that I can find the parent path
    // get the inode number for the parent node first
    pop_last_file_name(delete_path, file_name);
    int parent_num = read_path(disk,delete_path );
    // try to see whether the parent node be found
    // Parent can always be found
    if (parent_num == -1) {
        fprintf(stderr, "Parent directory dddoes not exist\n");
        exit(EISDIR);
    }

    // draft 1. find the dir inode and for each one dir dir_entry
    // do the regular deleteion as before if it is Directory
    // again do the regular deleteion again this will be a
    // recusrive call so I will need function that
    // can handle both file deleteion and directory deleteion
    // and recursivly called
    delete_inodes( disk, tem_inode, parent_num,
      file_name, S_ISDIR(tem_inode->i_mode));
}
