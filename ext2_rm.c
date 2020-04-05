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
    fprintf(stderr, "This command's Usage: %s disk_img abs_path\n", proginput);
}


int main(int argc, char **argv) {
    unsigned char* disk;

    // Check whether the input has exactly 3 input
    // if there are less than 3 argvs
    // If not return the error
    if (argc != 3) {
        show_usuage(argv[0]);
        exit(1);
    }

    // Check whether the destination path is a absolute path or not
    // If not, then terminate and show error
    if (argv[2][0] != '/' || argv[2][strlen(argv[2]) - 1] == '/') {
        show_usuage(argv[0]);
        exit(1);
    }

    // Read the image first
    disk = read_disk(argv[1]);

    // Parse the path and find inode number
    // Get the innode number
    int inode_num = read_path(disk, argv[2]);

    // if the inode number is -1
    // Then the directory can not be found
    if (inode_num == -1) {
        fprintf(stderr, "%s: No such file or directory\n", argv[0]);
        exit(ENOENT);
    }

    // get the inode
    struct ext2_inode *tem_inode = get_inode(disk, inode_num);

    // S_ISDIR returns non-zero if the file is a directory.
    if (S_ISDIR(tem_inode->i_mode)) {
        fprintf(stderr, "%s: %s is a directory\n", argv[0], argv[2]);
        exit(EISDIR);
    }

    // initialize a dir name
    char file_name[EXT2_NAME_LEN + 1];

    // cut the the repository so that I can find the parent path
    // get the inode number for the parent node first
    pop_last_file_name(argv[2], file_name);
    int parent_num = read_path(disk, argv[2]);
    // try to see whether the parent node be found
    // Parent can always be found
    if (parent_num == -1) {
        fprintf(stderr, "Parent directory does not exist\n");
        exit(EISDIR);
    }
    // Delete the inode and update the link correspondingly
    delete_inodes( disk, tem_inode, parent_num,
      file_name, S_ISDIR(tem_inode->i_mode));

}
