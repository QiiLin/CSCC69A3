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

// check whether the two path name are the same
int path_comparsion(char *s1, char *s2, int len) {
    int counter;
    for (counter = 0; counter < len; counter++) {
        if (s2[counter] == '\0' || s1[counter] != s2[counter]) {
            return 1;
        }
    }
    if (s2[counter] == '\0') {
        return 0;
    } else {
        return 1;
    }
}

// translate the length
int len_generator(int len) {
    int tem;
    // check whether the num is divisiable by 4
    // If so, then divide by 4
    // If not, then we need to add 1 in order to create a gap
    if (len % 4 == 0) {
        tem = len / 4;
    } else {
        tem = len / 4 + 1;
    }
    return tem * 4 + 8;
}

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
struct ext2_group_desc *get_group_desc(unsigned char *disk) {
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
}

// get the block bitmap
char *get_block_bitmap(unsigned char *disk) {
    struct ext2_group_desc *group_description = get_group_desc(disk);
    return (char *) get_block(disk, group_description->bg_block_bitmap);
}

// get the super block
struct ext2_super_block *get_super_block(unsigned char *disk) {
    return (struct ext2_super_block *) get_block(disk, 1);
}

// helper function for delete
// This function return 1 whenever we are able to delete the block from the disk
// Return 0 when we fail to delete the block
int delhelper(unsigned char *disk, int block_num, char *name) {
    // get the block using the given block number
    unsigned long pos = (unsigned long) get_block(disk, block_num);
    // This block is the parent block where we would like to delete
    struct ext2_dir_entry_2 *parent = (struct ext2_dir_entry_2 *) pos;
    // initialize the tem value for further
    int cur_len, len = 0, tem_len = 0;
    struct ext2_dir_entry_2 *curr = parent;
    unsigned long prev_pos = pos;
    do {
        // See whether the file name is current file
        // If so, no need to add the len for it
        if (path_comparsion(curr->name, name, curr->name_len) == 0) {
            len = curr->rec_len;
            // we have th current dir entry and prev dir_entry
            // if the found inode is at first dir entry -> impossible
            if (prev_pos == pos) {
              // you are trying to delete . which means you are trying to
              // delete the current directory
              // need to invoke the call to delete specified inode
              // some thing to consider for bonu
            } else {
                struct ext2_dir_entry_2 *prev_entry =
                (struct ext2_dir_entry_2 *) prev_pos;
                prev_entry->rec_len += len;
                unsigned int temp = curr->inode;
                curr->inode = 0;
                return temp;
            }
        }

        cur_len = curr->rec_len;
        tem_len += cur_len;
        prev_pos = pos;
        pos = pos + cur_len;
        curr = (struct ext2_dir_entry_2 *) pos;
    } while (pos % EXT2_BLOCK_SIZE != 0);

    // // if entry not found in the block
    // if (pos % EXT2_BLOCK_SIZE == 0)
    //     return 0;
    //
    // // dir entry is the last entry in dir
    // if (len != len_generator(curr->name_len)) {
    //     pos = pos - cur_len;
    //     curr = (struct ext2_dir_entry_2 *) pos;
    //     curr->rec_len += len;
    // } else {
    //     // dir entry in in middle, then move the memory.
    //     memmove((char *) pos, (char *) pos + len, EXT2_BLOCK_SIZE - len - tem_len);
    //     curr = (struct ext2_dir_entry_2 *) pos;
    //     do {
    //         cur_len = curr->rec_len;
    //         pos = pos + cur_len;
    //         curr = (struct ext2_dir_entry_2 *) pos;
    //     } while ((pos + len) % EXT2_BLOCK_SIZE != 0);
    //
    //     pos = pos - cur_len;
    //     curr = (struct ext2_dir_entry_2 *) pos;
    //     curr->rec_len += len;
    //
    // }
    return -1;
}

// Decrease the link
void decrease_link_count(unsigned char *disk, unsigned int *block, int blocks) {
    struct ext2_super_block *sb = get_super_block(disk);
    char *bmi = get_inode_bitmap(disk);
    // int inum[32];
    // inum[0] = 2;
    int counter, sec_counter, index = 0;
    for (counter = 0; counter < sb->s_inodes_count; counter++) {
        unsigned c = bmi[counter / 8];
        if ((c & (1 << index)) > 0 && counter > 10) {
            // inum[inumc++] = counter + 1;
            struct ext2_inode *curr = get_inode(disk, counter + 1);
            // check if the blocks are same
            if (curr->i_blocks == blocks) {
                int s_flag = 1;
                // for each block check if the block are the same
                for (sec_counter = 0; sec_counter < blocks && sec_counter < 13; sec_counter++) {
                    // one of the block are not the same then this is not the block we looking for
                    if (curr->i_block[sec_counter] != block[sec_counter]) {
                        s_flag = 0;
                        break;
                    }
                }
                if (s_flag == 1) {
                    if (0)
                        curr->i_links_count++;
                    else
                        curr->i_links_count--;
                }
            }
        }
        if (++index == 8)
            index = 0;
    }
}

void reset_bmap(int inode_num, char *bitmap) {
    inode_num = inode_num - 1;
    unsigned tem = bitmap[inode_num / 8];
    bitmap[inode_num / 8] = tem & ~(1 << (inode_num % 8));
}

int delete_blocks(unsigned char *disk, int inode_num) {
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE);
    struct ext2_inode *current = get_inode(disk, inode_num);
    int block_number;
    int i;
    int indirect_block_index;
    // used data block count
    int used_block = ((current->i_blocks)/(2<<sb->s_log_block_size));
    // to avoid go through extra block
    int used_data_block = used_block > 12 ? used_block - 1: used_block;
    for (i = 0; i < used_data_block; i++ ) {
      if (i < 12) {
        // direct case
        block_number = current->i_block[i];
      } else {
        // count in_direct_block as well
        if (i == 12 ) {
          indirect_block_index = current->i_block[12];
        }
        // indirect case
        int *in_dir = (int *) (disk + EXT2_BLOCK_SIZE * current->i_block[12]);
        block_number = in_dir[i - 12];
      }
      set_bitmap(1,disk, block_number, 0);
    }
    // if there is in_direct_block case
    if (used_block > 12) {
      set_bitmap(1, disk, indirect_block_index, 0);
    }
    current->i_dtime = time(NULL);
    return used_block;
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
    // get the parent inode
    struct ext2_inode *parent_inode = get_inode(disk, parent_num);
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // set the variable for deletion
    int counter;
    int block;
    int *tem_list;
    int target_inode = 0;
    // delete inode from repository
    for (counter = 0; counter <  ((parent_inode->i_blocks)/(2<<sb->s_log_block_size)); counter = counter + 1) {
        if (counter < 12) {
            block = parent_inode->i_block[counter];
        } else {
            tem_list = (int *) get_block(disk, parent_inode->i_block[12]);
            block = tem_list[counter - 12];
        }
        target_inode = delhelper(disk, block, file_name);
        // check if the entry is being removed
        if (target_inode > 0) {
            break;
        }
    }
    // remove the links from the parent inode
    // parent_inode->i_links_count--;
    // delete the link count from the inode
    if (target_inode == 0) {
      fprintf(stderr, "Unable to delete the file dir entry: %s \n",file_name );
      exit(1);
    }

    tem_inode->i_links_count --;
    // inode bitmap to 0
    set_bitmap(0, disk, target_inode, 0);
    struct ext2_group_desc *bgd = get_group_desc(disk);
    // check whether the there is not links anymore
    // if so, delete the block
    printf("%s   || %d  \n", "test", tem_inode->i_links_count);
    if (tem_inode->i_links_count == 0) {
        int blocks = delete_blocks(disk, inode_num);
            printf("%s   ||  blocks %d  \n", "test", blocks);
        bgd->bg_free_blocks_count -= (-blocks);
        sb->s_free_blocks_count -= (-blocks);
    } else {
        bgd->bg_free_blocks_count -= (-1);
        sb->s_free_blocks_count -= (-1);
    }
    bgd->bg_free_inodes_count -= (-1);
    sb->s_free_inodes_count -= (-1);

}
