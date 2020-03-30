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

// 

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




}
