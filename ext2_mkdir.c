#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"
#include <string.h>
#include <errno.h>
#include "ext2_helper.c"

int main(int argc, char **argv) {
  unsigned char *disk;
  char* current_path;
  if (argc != 3 || argv[2][0] != '/') {
    fprintf(stderr, "Usage: %s <disk_img path> <new directory's absolute path>\n", argv[0]);
    exit(1);
  }
  current_path = argv[2];
  disk = read_disk(argv[1]);
  int inode_index = read_path (disk, current_path);
  if (inode_index != -1) {
    fprintf(stderr, "%s: already exist\n", argv[2]);
    exit(EEXIST);
  }
  // loop through the path and remove the last directory in it
  // and store it somewhere,
  int path_length = strlen(current_path);
  char dir_name[path_length];
  // get file name and remove the last file/directory from the current_path
  pop_last_file_name(current_path, dir_name);
  // get the inode number fo the parent_directory
  inode_index = read_path(disk, current_path);
  // check if inode is found
  if (inode_index == -1) {
    fprintf(stderr, "%s: No such directory\n", argv[2]);
    exit(ENOENT);
  }
  // get the actual inode
  struct ext2_inode *place_inode = get_inode(disk, inode_index);
  // make sure it is a directory
  if (!S_ISDIR(place_inode->i_mode)) {
      fprintf(stderr, "%s: No such directory\n", argv[2]);
      exit(ENOENT);
  }

  // allocate inodes
  int free_inode_index = find_free_inode(disk);
  if (free_inode_index == -1) {
    fprintf(stderr, "%s: No inode avaiable\n", argv[0]);
    exit(1);
  }
  // allocate blocks
  int* free_blocks = find_free_blocks(disk, 1);
  if (free_blocks[0] == -1) {
    fprintf(stderr, "%s: No blocks avaiable\n", argv[0]);
    exit(1);
  }
  // update bitmap to make sure we are working on it for sure
  set_bitmap(0, disk, free_inode_index, 1);
  set_bitmap(1, disk, free_blocks[0], 1);
  // update the parent directory inode's dir_entry
  int result = add_link_to_dir(place_inode, disk, dir_name, free_inode_index,
      EXT2_FT_DIR);
  // if there isn't any blcok for the new dir_entry
  if (result == -1) {
    // need to revert the bitmap
    set_bitmap(0, disk, free_inode_index, 0);
    set_bitmap(1, disk, free_blocks[0], 0);
    fprintf(stderr, "%s: No blocks avaiable\n", argv[0]);
    exit(1);
  }
  place_inode->i_links_count++;
  // init inode amd set up property
  struct ext2_inode *dir_inode = initialize_inode(disk, free_inode_index,EXT2_S_IFDIR,EXT2_BLOCK_SIZE);
  // init block as dir_entry for the dir_inode
  unsigned long pos = (unsigned long) disk + EXT2_BLOCK_SIZE * free_blocks[0];
  // init first dir_entry for .
  struct ext2_dir_entry_2 *dir_entry = (struct ext2_dir_entry_2 *) pos;
  dir_entry->name[0] = '.';
  dir_entry->name_len = 1;
  dir_entry->inode = free_inode_index;
  dir_entry->rec_len = 12;
  dir_entry->file_type = EXT2_FT_DIR;
  // init first dir_entry for ..
  pos = pos + dir_entry->rec_len;
  dir_entry = (struct ext2_dir_entry_2 *) pos;
  dir_entry->name[0] = '.';
  dir_entry->name[1] = '.';
  dir_entry->name_len = 2;
  dir_entry->inode = inode_index;
  dir_entry->rec_len = EXT2_BLOCK_SIZE - 12;
  dir_entry->file_type = EXT2_FT_DIR;
  // update the link count of the directory and others blocks
  dir_inode->i_links_count ++;
  dir_inode->i_block[0] = free_blocks[0];
  dir_inode->i_blocks = 2;
  // udpate the group descriptor
  // update used block and inodes
  struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
  // get the inode bitmap to manipulate its bits if there are free inode in the bitmap
  struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
  bgd->bg_free_inodes_count --;
  sb->s_free_inodes_count --;
  bgd->bg_free_blocks_count --;
  sb->s_free_blocks_count  --;
  bgd->bg_used_dirs_count ++;
}
