#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2_helper.c"


unsigned char *disk;

int main(int argc, char **argv) {
    if(argc != 4 || argv[3][0] != '/') {
        fprintf(stderr, "Usage: ext2_cp <ext2 image file name> <file path on your OS> <abs file path on image>\n");
        exit(1);
    }
    FILE * fp = fopen(argv[2], "r");
    if (fp == NULL) {
        perror("Reading local file %s unsuccessfully");
        exit(ENOENT);
    }
    // step 1: read the disk
    disk = read_disk(argv[1]);
    // step 2: get info from the file
    struct stat fileInfo;
    if (stat(argv[2], &fileInfo) != 0) {
        perror("file was not found");
        exit(ENOENT);
    }
    if (!S_ISREG(fileInfo.st_mode)) {
        fprintf(stderr, "%s: wrong file type given\n", argv[2]);
        exit(1);
    }
    // by defn the file name is the name from the input file
    char *file_name = get_file_name(argv[2]);
    // check if the path passed in is a valid asbolute path leading to a directory
    // int is_path = check_valid_path(inum, inumc, dirs, dirsin, sb, bmi, bgd, in, argv[3]);
    int dir_inode = read_path(disk, argv[3]);
    // init dir_name
    char* new_file_name;
    struct ext2_inode *place_inode;
    // check if the path can be found
    if (dir_inode == -1) {
      char dir_name[EXT2_NAME_LEN];
      // if the target path is looking for directory
      if (argv[3][strlen(argv[3]) - 1] == '/') {
          fprintf(stderr, "%s: No such directory\n", argv[3]);
          exit(ENOENT);
      }
      // this will modified the path to the parent directory
      // and save the name of new file name into the dir_name
      pop_last_file_name(argv[3], dir_name);
      // do another check if the parent directory exist
      dir_inode = read_path(disk, argv[3]);
      if (dir_inode == -1) {
        fprintf(stderr, "%s: No such file or directory\n", argv[3]);
        exit(ENOENT);
      }
      place_inode = get_inode(disk, dir_inode);
      // if it is not a directory
      if (!S_ISDIR(place_inode->i_mode)) {
        fprintf(stderr, "%s: No such directory\n", argv[3]);
        exit(ENOENT);
      }
      new_file_name = dir_name;
    } else {
      new_file_name = file_name;
      place_inode = get_inode(disk, dir_inode);
      if (!S_ISDIR(place_inode->i_mode)) {
        fprintf(stderr, "%s: No such directory\n", argv[3]);
        exit(ENOENT);
      }
    }
    printf("is_path variable value is %d\n", dir_inode);
    // By Qi: it is not longer needed since check valid path will handle it
    // if (dir_inode < 0) {
    //     fprintf(stderr, "No such file or directory\n");
    //     exit(ENOENT);
    // }
    // get file byte size
    long file_size = fileInfo.st_size;
    char *file_contents = readFileBytes(argv[2]);
    printf("file size is %ld\n", file_size);
    printf("file name is %s, the length of file name is %lu\n", file_name, strlen(file_name));
    printf("file contents is %s\n", file_contents);
    printf("length of contents is %lu\n", strlen(file_contents));
    printf("fila names is %s\n", file_name);
    int valid_filename = check_valid_file(disk, dir_inode, file_name);
    if (valid_filename < 0) {
        fprintf(stderr, "A file named %s already exists in the directory",file_name);
        exit(EEXIST);
    }
    int current_free_blocks = num_free_blocks(disk);
    int current_free_inodes = num_free_inodes(disk);
    printf("    free blocks: %d\n", current_free_blocks);
    printf("    free inodes: %d\n", current_free_inodes);

    // start the operation
    // Step 1: make sure we have enough block and inode to perform this
    // allocate inodes
    int free_inode_index = find_free_inode(disk);
    if (free_inode_index == -1) {
      fprintf(stderr, "%s: No inode avaiable\n", argv[0]);
      exit(1);
    }
    int required_block = get_required_block(file_size);
    int indir_block = 0;
    // check if it need redirect block
    if (required_block > 12) {
      indir_block = 1;
    }
    printf("%d  need blocks \n", required_block);
    int* free_blocks = find_free_blocks(disk, required_block + indir_block);
    if (free_blocks[0] == -1) {
      fprintf(stderr, "%s: No blocks avaiable\n", argv[0]);
      exit(1);
    }
    set_bitmap(0, disk, free_inode_index, 1);
    for (int i = 0; i < required_block + indir_block; i++) {
      set_bitmap(1, disk, free_blocks[i], 1);
    }
    // update the directory
    int result = add_link_to_dir(place_inode, disk, new_file_name, free_inode_index,
        EXT2_FT_REG_FILE);
    if (result == -1) {
      // need to revert the bitmap
      set_bitmap(0, disk, free_inode_index, 0);
      for (int i = 0; i < required_block + indir_block; i++) {
        set_bitmap(1, disk, free_blocks[i], 1);
      }
      fprintf(stderr, "%s: No blocks avaiable\n", argv[0]);
      exit(1);
    }
    printf("have found a free inode at inodenum %d\n", free_inode_index);
    printf("has enough blocks? %d\n", required_block);
    // Step 2: start create inodes and update it property
    struct ext2_inode *file_inode = initialize_inode(disk, free_inode_index,EXT2_S_IFREG, file_size);
    file_inode->i_blocks = required_block*2;
    // step 3: open file Buffer
    char tmp_buffer[EXT2_BLOCK_SIZE+1];
    unsigned int * indirect_block;
    // step 4: create block and fill the block in there first
    for(int i = 0; i < required_block; i++) {
      if (i < 12) {
        file_inode->i_block[i] = free_blocks[i];
      } else {
        // if it reach here then the indir_block must equal to 1
        // so the  free_blocks[required_block]; exist
        if (i == 12 ) {
          // first time access inderect so we need init the indirect_block
          file_inode->i_block[12] = free_blocks[required_block];
          indirect_block = (unsigned int *)(disk + file_inode->i_block[12] * EXT2_BLOCK_SIZE);
        }
        indirect_block[i-12] = free_blocks[i];
      }
    }
    int block_number;
    // step 5: start memcpy stuff into it
    for(int i = 0; i < required_block; i++) {
      if (i < 12) {
        // direct case
        block_number = file_inode->i_block[i];
      } else {
        // indirect case
        int *in_dir = (int *) (disk + EXT2_BLOCK_SIZE * file_inode->i_block[12]);
        block_number = in_dir[i - 12];
      }
      // // go through the buffer and copy the right proption of data
      // // Note: we can't just copy exact block size ... that is not needed here
      // // read through each char and place them into the buffer
      // int single_char;
      // int copy_size = 0;
      // while ((single_char = getc(fp)) != EOF) {
      //     tmp_buffer[copy_size] = single_char;
      //     copy_size = copy_size + 1;
      //     if (copy_size == EXT2_BLOCK_SIZE) {
      //       break;
      //     }
      // }
      // tmp_buffer[copy_size] = '\0';
      // // at null terminated for the last entry
      // if (copy_size < EXT2_BLOCK_SIZE) {
      //   copy_size ++;
      // }
      // printf("%d len \n", copy_size);
      // // after that place the stuff in the buffer into the memo
      unsigned int *block = (unsigned int *)(disk + block_number * EXT2_BLOCK_SIZE);
      // memcpy(block, tmp_buffer, copy_size);
      char* result = fgets(tmp_buffer, EXT2_BLOCK_SIZE+1, fp);
      if (result == NULL) {
        printf("%s\n","Something fk up" );
      }
      memcpy(block, tmp_buffer, EXT2_BLOCK_SIZE);
    }
    // update used block and inodes
    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    // get the inode bitmap to manipulate its bits if there are free inode in the bitmap
    struct ext2_group_desc *bgd = (struct ext2_group_desc *) (disk + 2048);
    bgd->bg_free_inodes_count--;
    sb->s_free_inodes_count--;
    bgd->bg_free_blocks_count -= (required_block + indir_block);
    sb->s_free_blocks_count -= (required_block + indir_block);
    return 0;
}
