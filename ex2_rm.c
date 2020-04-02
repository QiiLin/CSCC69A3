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

/* Create a char pointer for the disk image loaded in the memory
*/
unsigned char *disk;

/* Helper function for invalid input
*/
void show_usuage(char *proginput) {
    fprintf(stderr, "This command's Usage: %s disk_img abs_path\n", proginput);
}

// Helper function to read the block
unsigned char *get_block(unsigned char*disk, int block_num) {
    return disk + EXT2_BLOCK_SIZE * block_num;
}

// Helper function to read group description
struct ext2_group_desc *get_group_desc(unsigned char *char) {
    return (struct ext2_group_desc *) get_block(disk, 2);
}

// get innode table
struct ext2_inode *get_inode_table(unsigned char *disk) {
    struct ext2_group_desc *group_descrption = get_group_desc(disk);
    return (struct ext2_inode *) get_block(disk, group_descrption->bg_inode_table);
}

// get the bitmap
char *get_inode_bitmap(unsigned char *disk) {
    struct ext2_group_desc *group_description = get_group_desc(disk);
    return (char *) get_block(disk, group_description->bg_inode_bitmap);

// get the block bitmap
char *get_block_bitmap(unsigned char *disk) {
    struct ext2_group_desc *group_description = get_group_desc(disk);
    return (struct ext2_super_block *) get_block(disk, 1);
}

// get the super block
struct ext2_super_block *get_super_block(unsigned char *disk) {
    return (struct ext2_super_block *) get_block(disk, 1);
}

// helper function
int delhelper(unsigned char *disk, int block_num, char *name) {
    unsigned long pos = (unsigned long) get_block(disk, block_num);
    struct ext2_dir_entry_2 *parent = (struct ext2_dir_entry_2 *) pos;

    int cur_len, len = 0, prefix = 0;
    struct ext2_dir_entry_2 *curr = parent;

    do {
        if (pathcmp(curr->name, name, curr->name_len) == 0) {
            len = curr->rec_len;
            break;
        }

        cur_len = curr->rec_len;
        prefix += cur_len;

        pos = pos + cur_len;
        curr = (struct ext2_dir_entry_2 *) pos;
    } while (pos % EXT2_BLOCK_SIZE != 0);

    // if entry not found in the block
    if (pos % EXT2_BLOCK_SIZE == 0)
        return 0;

    // dir entry is the last entry in dir
    if (len != find_rec_len(curr->name_len)) {
        pos = pos - cur_len;
        curr = (struct ext2_dir_entry_2 *) pos;
        curr->rec_len += len;
    } else {
        // dir entry in in middle, then move the memory.
        memmove((char *) pos, (char *) pos + len, EXT2_BLOCK_SIZE - len - prefix);
        curr = (struct ext2_dir_entry_2 *) pos;
        do {
            cur_len = curr->rec_len;
            pos = pos + cur_len;
            curr = (struct ext2_dir_entry_2 *) pos;
        } while ((pos + len) % EXT2_BLOCK_SIZE != 0);

        pos = pos - cur_len;
        curr = (struct ext2_dir_entry_2 *) pos;
        curr->rec_len += len;

    }
    return 1;
}

// Decrease the link
void decrease_link_count(unsigned char *disk, unsigned int *block, int blocks) {
    struct ext2_super_block *sb = get_super_block(disk);
    char *bmi = get_inode_bitmap(disk);
    int inum[32];
    inum[0] = 2;
    int i, j, index = 0, inumc = 1;
    for (i = 0; i < sb->s_inodes_count; i++) {
        unsigned c = bmi[i / 8];
        if ((c & (1 << index)) > 0 && i > 10) {
            inum[inumc++] = i + 1;
        }
        if (++index == 8)
            index = 0;
    }

    for (i = 0; i < inumc; i++) {
        struct ext2_inode *curr = get_inode(disk, inum[i]);
        if (curr->i_blocks == blocks) {
            int same = 1;
            for (j = 0; j < blocks && j < 13; j++) {
                if (curr->i_block[j] != block[j]) {
                    same = 0;
                    break;
                }
            }
            if (same == 1) {
                if (0)
                    curr->i_links_count++;
                else
                    curr->i_links_count--;
            }
        }
    }
}

void reset_bmap(int inode_num, char *bitmap) {
    inode_num = inode_num - 1;
    unsigned tem = bitmap[inode_num / 8];
    bitmap[inode_num / 8] = tem & ~(1 << (inode_num % 8));
}

int delete_blocks(unsigned char *disk, int inode_num) {
    int i, blocks = 0;
    char *bm = get_block_bitmap(disk);
    struct ext2_inode *p = get_inode(disk, inode_num);
    for (i = 0; i < p->i_blocks; i++) {
        if (i >= 12)
            break;
        reset_bitmap(bm, p->i_block[i]);
        blocks++;
    }

    if (i < p->i_blocks) {
        int *block = (int *) get_block(disk, p->i_block[12]);
        for (; i < p->i_blocks; i++) {
            reset_bitmap(bm, block[i - 12]);
            blocks++;
        }
        reset_bitmap(bm, p->i_block[12]);
        blocks++;
    }

    p->i_dtime = time(NULL);

    return blocks;
}

int main(int argc, char **argv) {
    
    // Check whether the input has exactly 3 input
    // if there are less than 3 argvs
    // If not return the error
    if (argc != 3) {
        show_usuage(argv[0]);
        exit(1);
    }

    // Check whether the destination path is a absolute path or not
    // If not, then terminate and show error
    if (argv[2][0] != '/') {
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
    if (node_num == -1) {
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

    // initialize a name container
    char name[EXT2_NAME_LEN + 1];
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
    // get the parent inode
    struct ext2_inode *parent_inode = get_inode(disk, parent_inode_num);

    // set the variable for deletion
    int counter;
    int block;
    int *tem_list;
    
    // delete inode from repository
    for (counter = 0; counter < (parent_inode->i_blocks); counter = counter + 1) {
        if (counter < 12) {
            block = parent_inode->i_block[counter];
        } else {
            tem_list = (int *) get_block(disk, parent_inode->i_block[12]);
            block = tem_list[i - 12];
        }
        if (delhelper(disk, block, name) != 0) {
            break;
        } 
    }
    // remove the links from the parent inode
    parent_inode->i_links_counts--;
    // delete the link count from the inode
    decrease_link_count(disk, tem_inode->i_block, tem_inode->i_blocks);

    // reset the bitmap
    reset_bmap(inode_num, get_inode_bitmap(disk));

    // check whether the there is not links anymore
    // if so, delete the block
    if (tem_inode->i_links_count == 0) {
        int blocks = delete_blocks(disk, inode_num);
        struct ext2_group_desc *tem = get_group_desc(disk);
        tem->bg_free_blocks_count -= (-1 - blocks);
    } else {
        struct ext2_group_desc *tem = get_group_desc(disk);
        tem->bg_free_blocks_count -= (-1);
    }
}
