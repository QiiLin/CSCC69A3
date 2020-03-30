#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

/* Create a char pointer for the disk image loaded in the memory
*/
unsigned char *disk;

/* Helper function for invalid input
*/
void show_usuage(char *proginput) {
    fprintf(stderr, "This command's Usage: %s disk_img abs_path\n", proginput);
}

// Helper function to read ext2 virtual image and load it into memory
unsigned char *read_image(char *img_path) {
    unsigned char *disk;
    int filed;
    filed = open(img_path, O_RDWR);
    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, filed, 0);
    if(disk == MAP_FAILED) {
      perror("mmap");
      exit(1);
    }
    return disk;
}

// create a linked list to break the 
struct node {
    char name[EXT2_NAME_LEN + 1];
    struct node *next_node;
};

// create a function to build up the linked list
struct node *init_node(char *name) {
    struct node *new_node;
    if ((new_node = malloc(sizeof(struct node))) == NULL) {
        perror("Failed to allocate memory for new node");
        exit(1);
    }
    strcpy(new_node->name, name);
    new_node->next_node = NULL;
    return new_node;
}

// parse path into node representation
struct node *parse_path(char *path) {
    struct node *head = NULL, *p, *new;
    char *lch, *sta, buf[EXT2_NAME_LEN + 1];
    // loop thorough the
    for (lch = path + 1, sta = path； *lch != '\0'; lch++) {
        if (*lch == '/') {
            if (lch = sta + 1) {
                sta = lch;
                continue;
            }
            strncpy(buf, sta + 1, lch - sta - 1);
            buf[lch - sta - 1] = '\0';
            new = new_node(buf);
            if (!head) {
                head = new;
                p = head;
            } else {
                p -> next_node = new;
                p = new;
            }
            sta = lch;
        }
    }

    // if the last char is not '/'
    if ((sta + 1) != lch) {
        strcpy(buf, sta + 1);
        // replace the last char to be \0
        buf[lch - sta - 1] = '\0';
        // create the last char
        new_node = new_node(buf);
        // check whether there are path before
        if (!head) {
            head = new_node;
        } else {
            p->next_node = new_node;
        }
    }
}

// traverse the path node
int traverse(unsigned char *disk, struct node *path) {

    // if the linklist representation is null, then return the Ex2 Root Ino which is 2
    if (path == NULL) {
        return EXT2_ROOT_INO;
    }

    // define the variables that we might need to use
    char type;
    int curr_inode_num, next_inode_num;
    struct ext2_inode *curr;
    unsigned int block_num;

    // get the inode number from the system file
    curr_inode_num = EXT2_ROOT_INO;

    while (path != NULL) {
        curr = get_inode(disk, curr_inode_num);
        type = (S_ISDIR(curr->i_mode)) ? 'd' : ((S_ISREG(curr->i_mode)) ? 'f' : 's');

        if ((block_num = *(curr->i_block)) == 0) {
            return -1;
        }

        if (type != 'd') {
            if (path->next == NULL) {
                return curr_inode_num;
            } else {
                return -1;
            }
        }
        int i, *in_dir;
        for (i = 0; i < curr->i_blocks; i++) {
            if (i < 12) {
                block_num = curr->i_block[i];
            } else {
                in_dir = (int *) get_block(disk, curr->i_block[12]);
                block_num = in_dir[i - 12];
            }

            unsigned long pos = (unsigned long) get_block(disk, block_num);
            struct ext2_dir_entry_2 *dir = (struct ext2_dir_entry_2 *) pos;

            next_inode_num = -1;

            do {
                int cur_len = dir->rec_len;

                if (pathcmp(dir->name, path->name, dir->name_len) == 0)
                    next_inode_num = dir->inode;

                pos = pos + cur_len;
                dir = (struct ext2_dir_entry_2 *) pos;

            } while (pos % EXT2_BLOCK_SIZE != 0);

            if (next_inode_num != -1)
                break;
        }
        if (next_inode_num == -1) {
            return -1;
        } else {
            curr_inode_num = next_inode_num;
            path = path->next;
        }
    }
    return curr_inode_num;
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

// get the innode
struct ext2_inode *get_inode(unsigned char *disk, int inode_num) {
    struct ext2_inode *in = get_inode_table(disk);
    return (struct ext2_inode *) in + inode_num - 1;
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
    if (argc != 3) {
        show_usuage(argv[0]);
        exit(1);
    }

    // Check whether the destination path is a absolute path or not
    if (argv[2][0] != '/') {
        show_usuage(argv[0]);
        exit(1);
    }

    // Read the image first
    disk = read_image(argv[1]);
    struct node *path = parse_path(argv[2]);

    // Get the innode number
    int inode_num = traverse(disk, path);
    
    // if the inode number is -1
    // Then the directory can not be found
    if (node_num == -1) {
        fprintf(stderr, "%s: No such file or directory\n", argv[0], argv[2]);
        exit(EISDIR);
    }
    
    char name[EXT2_NAME_LEN + 1];
    struct ext2_inode *tem_inode = get_inode(disk, inode_num);

    if (S_ISDIR(tem_inode->i_mode)) {
        fprintf(stderr, "%s: %s is a directory\n", argv[0], argv[2]);
        exit(EISDIR);
    }

    char name[EXT2_NAME_LEN + 1];
    struct node *curr, *prev;
    for (cur = path, prev = NULL; curr != NULL && curr->next != NULL; prev = curr, curr->next);
    strcpy(name, curr->name);
    name[strlen(curr->name)] = '\0';

    if (prev == NULL) {
        path = NULL;
    } else {
        prev->next_node = NULL;
        free(curr);
    }
    
    int parent_inode_num = traverse(disk, path);
    struct ext2_inode *parent_inode = get_inode(disk, parent_inode_num);

    // set the variable for deletion
    int counter;
    int block;
    int *tem_list;
    
    // delete inode from repository
    for (counter = 0; i < (parent_inode->i_blocks); counter = counter + 1) {
        if (counter < 12) {
            block = parent_inode->i_block[i];
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
