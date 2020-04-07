#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include "ext2.h"
#include <errno.h>

// General Use function
// Get functions
unsigned char *get_block(unsigned char*disk, int block_num);
struct ext2_group_desc *get_group_desc(unsigned char *disk);
struct ext2_inode *get_inode_table(unsigned char *disk);
char *get_inode_bitmap(unsigned char *disk);
char *get_block_bitmap(unsigned char *disk);
struct ext2_inode *get_inode (unsigned char *disk, int inodenum);
struct ext2_super_block *get_super_block(unsigned char *disk);
int delete_blocks(unsigned char *disk, struct ext2_inode* current);
void delete_all_entry(unsigned char* disk, struct ext2_super_block *sb, int target_inode, struct ext2_inode* tem_inode);
int delete_inodes(unsigned char* disk, struct ext2_inode* tem_inode, int parent_num, char* file_name, int mode);
char* parse_path(char* target_path);
char* get_file_name(char *file_path);
unsigned char * read_disk(char *image_path);
int compare_path_name (char *s1, char *s2, int len);
int delete_inodes(unsigned char* disk, struct ext2_inode* tem_inode, int parent_num, char* file_name, int mode);
void delete_all_entry(unsigned char* disk, struct ext2_super_block *sb, int target_inode, struct ext2_inode* tem_inode);
int check_valid_file(unsigned char *disk, int dir_inodenum, char *file_name);
char* parse_path(char* target_path);
struct ext2_inode *get_inode (unsigned char *disk, int inodenum);
int delete_blocks(unsigned char *disk, struct ext2_inode* current);
char* get_file_name(char *file_path);
char* readFileBytes(const char *name);
int num_free_blocks (unsigned char *disk);
int num_free_inodes (unsigned char *disk);
int get_required_block(int file_size);
int read_path(unsigned char* disk, char* arg_path);
int get_min_rec_len(int name_len);
int set_bitmap(int mode, unsigned char *disk, int index, int value);
int find_free_inode(unsigned char *disk);
int *find_free_blocks(unsigned char *disk, int require_block);
int add_link_to_dir(struct ext2_inode* place_inode, unsigned char* disk, char* dir_name, unsigned int input_inode, unsigned char block_type);
void pop_last_file_name(char *file_path, char* dir_name);
struct ext2_inode * initialize_inode(unsigned char* disk, int inode_num, unsigned short type, int size);
void substr(char * path, int i , int j, char* res);
void remove_ending_slash(char* path);