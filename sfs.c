#include "disk.h"
#include "sfs.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// Bit operations perforemed defined here
#define SetBit(A, k) (A[(k / 32)] |= (1 << (k % 32)))
#define ClearBit(A, k) (A[(k / 32)] &= ~(1 << (k % 32)))
#define TestBit(A, k) (A[(k / 32)] & (1 << (k % 32)))
disk *stored_diskpointer = NULL;
#define Mounted 1
#define Unmounted 0

int create_file();

void *temp_data_block;
int Filesystem_State;
typedef struct directory_struct
{
      uint32_t valid;
      uint32_t type;
      char file_name[20];
      uint32_t length;
      uint32_t inumber;
} directory_struct;
///////////////////////////////////////////////////////////////////////
// format function
// Arguments : 1. Disk Pointer
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int format(disk *diskptr)
{
      super_block superblock;
      int M, N, I, IB, R, DBB, DB;
      N = diskptr->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      DBB = ceil((double)R / (8 * BLOCKSIZE));
      DB = R - DBB;
      superblock.magic_number = 12345;
      superblock.blocks = M;
      superblock.inode_blocks = I;
      superblock.inodes = I * 128;
      superblock.inode_bitmap_block_idx = 1;
      superblock.inode_block_idx = 1 + IB + DBB;
      superblock.data_block_bitmap_idx = 1 + IB;
      superblock.data_block_idx = 1 + IB + DBB + I;
      superblock.data_blocks = DB;
      temp_data_block = (void *)malloc(BLOCKSIZE);
      int return_val;
      return_val = write_block(diskptr, 0, &superblock);
      if (return_val == -1)
      {
            printf("Write Superblock Failed.\n");
            return -1;
      }
      uint32_t bitmap[1024];
      for (int iterator = 0; iterator < 32768; iterator++)
      {
            ClearBit(bitmap, iterator);
      }
      for (int iterator = 0; iterator < IB; iterator++)
      {
            return_val = write_block(diskptr, superblock.inode_bitmap_block_idx + iterator, bitmap);
            if (return_val == -1)
            {
                  printf("Inode Bitmap Initialization failure.\n");
                  return -1;
            }
      }
      for (int iterator = 0; iterator < DBB; iterator++)
      {
            return_val = write_block(diskptr, superblock.data_block_bitmap_idx + iterator, bitmap);
            if (return_val == -1)
            {
                  printf("Inode Bitmap Initialization failure.\n");
                  return -1;
            }
      }
      for (int iterator = 0; iterator < I; iterator++)
      {
            inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
            inode node = {
                .valid = 0,
                .size = -1,
                .direct = {-1, -1, -1, -1, -1},
                .indirect = -1};

            for (int inode_idx = 0; inode_idx < 128; inode_idx++)
            {
                  inode_block_array[inode_idx] = node;
            }
            return_val = write_block(diskptr, superblock.inode_block_idx + iterator, inode_block_array);
            if (return_val == -1)
            {
                  printf("Inode Initialization failure.\n");
                  return -1;
            }
      }
      Filesystem_State = Unmounted;
      return 0;
}
///////////////////////////////////////////////////////////////////////
// mount function
// Arguments : 1. Disk Pointer
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int mount(disk *diskptr)
{
      stored_diskpointer = diskptr;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed\n");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      if (temp_super_block.magic_number != MAGIC)
      {
            printf("Invalid Magic Number. Filesystem cannot be mounted\n");
            return -1;
      }
      Filesystem_State = Mounted;
      create_file();
      printf("Mount Succesfull\n");
      return 0;
}
///////////////////////////////////////////////////////////////////////
// create_file function
// Arguments : NULL
//
// Returns: Inode Number of file -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int create_file()
{
      int M, N, I, IB;
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      inode node = {
          .valid = 1,
          .size = 0,
          .direct = {-1, -1, -1, -1, -1},
          .indirect = -1};
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int offs = -1;
      int in_block_n = -1;
      uint32_t bitmap[1024];
      int flag = 0;

      for (int iterator = 0; iterator < IB; iterator++)
      {
            return_val = read_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + iterator, bitmap);
            if (return_val == -1)
            {
                  printf("Read Inode Bitmap Failure\n");
                  return -1;
            }
            for (int j = 0; j < 32768; j++)
            {
                  if (!TestBit(bitmap, j))
                  {
                        SetBit(bitmap, j);
                        offs = j;
                        flag = 1;
                        in_block_n = iterator;
                        break;
                  }
            }
            if (flag == 1)
                  break;
      }
      return_val = write_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + in_block_n, bitmap);
      if (return_val == -1)
      {
            printf("Create file failure while writing inode Bitmap\n");
            return -1;
      }
      int inode_no = in_block_n * 32768 + offs;
      int inode_block_no = inode_no / 128;
      int inode_offs = inode_no % 128;

      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Create file failure while reading inode Bitmap \n");
            return -1;
      }
      inode_block_array[inode_offs] = node;
      return_val = write_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Create file failure while writing inode Bitmap \n");
            return -1;
      }
      return inode_no;
}
///////////////////////////////////////////////////////////////////////
// remove_file function
// Arguments : 1. Inode Number of file
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int remove_file(int inumber)
{
      int M, N, I, IB, R;
      uint32_t bitmap[1024];
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      inode node = {
          .valid = 0,
          .size = 0,
          .direct = {-1, -1, -1, -1, -1},
          .indirect = -1};
      inode temp_store_node;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed\n");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int bitpos = inumber / 32768;
      int bitoffs = inumber % 32768;
      if (bitpos < 0 || inumber > I * 128)
      {
            printf("Failure in Remove File because of Wrong Inode NO.\n");
            return -1;
      }
      return_val = read_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + bitpos, bitmap);
      if (return_val == -1)
      {
            printf("Failure in Remove File because of Write Inode Bitmap Read\n");
            return -1;
      }
      if (!TestBit(bitmap, bitoffs))
      {
            printf("Failure in Remove File because Inode is not set\n");
            return -1;
      }

      int inode_block_no = inumber / 128;
      int inode_offs = inumber % 128;
      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in Remove File because of Inode Bitmap read\n");
            return -1;
      }
      temp_store_node = inode_block_array[inode_offs];
      inode_block_array[inode_offs] = node;
      return_val = write_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in Remove File because of Inode Bitmap write\n");
            return -1;
      }
      ClearBit(bitmap, bitoffs);
      return_val = write_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + bitpos, bitmap);
      if (return_val == -1)
      {
            printf("Failure in Remove File because of Inode Bitmap Write\n");
            return -1;
      }
      uint32_t indirect[1024];
      if (temp_store_node.size > 0)
      {
            int end_idx = ceil((double)temp_store_node.size / BLOCKSIZE), start_idx = 0;
            if (end_idx < 5)
            {
                  for (; start_idx < end_idx; start_idx++)
                  {
                        int datapos = temp_store_node.direct[start_idx] / 32768;
                        int dataoffs = temp_store_node.direct[start_idx] % 32768;
                        return_val = read_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                        if (return_val == -1)
                        {
                              printf("Failure in Remove File\n");
                              return -1;
                        }
                        ClearBit(bitmap, dataoffs);
                        return_val = write_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                        if (return_val == -1)
                        {
                              printf("Failure in Remove File \n");
                              return -1;
                        }
                  }
                  return 0;
            }
            for (; start_idx <= 4; start_idx++)
            {
                  int datapos = temp_store_node.direct[start_idx] / 32768;
                  int dataoffs = temp_store_node.direct[start_idx] % 32768;
                  return_val = read_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                  if (return_val == -1)
                  {
                        printf("Failure in Remove File \n");
                        return -1;
                  }
                  ClearBit(bitmap, dataoffs);
                  return_val = write_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                  if (return_val == -1)
                  {
                        printf("Failure in Remove File \n");
                        return -1;
                  }
            }
            if (read_block(stored_diskpointer, temp_store_node.indirect + temp_super_block.data_block_idx, indirect) < 0)
            {
                  printf("Failure in Remove File \n");
                  return -1;
            }
            int counter = 0;
            while (start_idx < end_idx)
            {
                  int datapos = indirect[counter] / 32768;
                  int dataoffs = indirect[counter] % 32768;
                  return_val = read_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                  if (return_val == -1)
                  {
                        printf("Failure in Remove File \n");
                        return -1;
                  }
                  ClearBit(bitmap, dataoffs);
                  return_val = write_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                  if (return_val == -1)
                  {
                        printf("Failure in Remove File \n");
                        return -1;
                  }
                  counter++;
                  start_idx++;
            }
      }
      return 0;
}
///////////////////////////////////////////////////////////////////////
// clearemptyblocks function
// Arguments : 1.Number of blocks
//
// Returns: Void
//////////////////////////////////////////////////////////////////////
void clearemptyblocks(int block_num)
{
      int M, N, I, IB, R, return_val;
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return;
      }
      uint32_t bitmap[1024];
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int clear_block_no = block_num / 32768;
      int clear_offset_no = block_num / 32768;
      return_val = read_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + clear_block_no, bitmap);
      if (return_val == -1)
      {
            printf("Failure while reading data bitmap \n");
            return;
      }
      ClearBit(bitmap, clear_offset_no);
      return_val = write_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + clear_block_no, bitmap);
      if (return_val == -1)
      {
            printf("Failure while writing data bitmap \n");
            return;
      }
      return;
}
///////////////////////////////////////////////////////////////////////
// emptyblocklist function
// Arguments : 1. Required number of blocks
//
// Returns: NULL -> For Failure
//		Pointer to array containing block numbers -> For Success
//////////////////////////////////////////////////////////////////////
int *emptyblocklist(int req_num_blocks)
{
      int M, N, I, IB, R, DBB, DB = 0, return_val;
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      DBB = ceil((double)R / (8 * BLOCKSIZE));
      int *block_ids = malloc(req_num_blocks * sizeof(int));
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return NULL;
      }
      uint32_t bitmap[1024];
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int idx = 0;
      for (int iterator = 0; iterator < DBB; iterator++)
      {
            return_val = read_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + iterator, bitmap);
            if (return_val == -1)
            {
                  printf("Failure while reading data bitmap \n");
                  return NULL;
            }
            for (int j = 0; j < 32768 && idx < req_num_blocks; j++)
            {
                  if (DB == temp_super_block.data_blocks)
                        break;
                  if (!TestBit(bitmap, j))
                  {
                        SetBit(bitmap, j);
                        block_ids[idx++] = iterator * 32768 + j;
                  }
                  DB++;
            }
            return_val = write_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + iterator, bitmap);
            if (return_val == -1)
            {
                  printf("Failure while writing data bitmap \n");
                  return NULL;
            }
            if (DB == temp_super_block.data_blocks)
                  break;
      }
      return block_ids;
}
///////////////////////////////////////////////////////////////////////
// stat function
// Arguments : 1. Inode Number of file
//
// Returns: -1 -> For Failure
//		0 -> For Success
// Prints : Stats of the file
//////////////////////////////////////////////////////////////////////
int stat(int inumber)
{
      int M, N, I, IB, R;
      int bitmap[1024];
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int bitpos = inumber / 32768;
      int bitoffs = inumber % 32768;
      if (bitpos < 0 || inumber > I * 128)
      {
            printf("Failure in stat because of wrong inode nunmber.\n");
            return -1;
      }
      return_val = read_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + bitpos, bitmap);
      if (return_val == -1)
      {
            printf("Failure in stat because of inode bitmap read\n");
            return -1;
      }
      if (!TestBit(bitmap, bitoffs))
      {
            printf("Failure in stat.\n");
            return -1;
      }
      int inode_block_no = inumber / 128;
      int inode_offs = inumber % 128;
      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in stat.\n");
            return -1;
      }
      inode node = inode_block_array[inode_offs];
      uint32_t size = node.size;
      uint32_t directpointers, indirectpointers, datablocks;
      if (size > 5 * BLOCKSIZE)
      {
            directpointers = 5;
      }
      else
            directpointers = ceil((double)size / BLOCKSIZE);
      if (size > 5 * BLOCKSIZE)
      {
            indirectpointers = ceil((double)size / BLOCKSIZE) - 5;
      }
      else
            indirectpointers = 0;
      if (indirectpointers == 0)
      {
            datablocks = directpointers;
      }
      else
            datablocks = directpointers + indirectpointers + 1;
      printf("Logical Size: %d\nNumber of Data Blocks in Use: %d\nNumber of direct pointers: %d\nNumber of indirect pointers: %d\n", size, datablocks, directpointers, indirectpointers);
      return 0;
}
///////////////////////////////////////////////////////////////////////
// read_i function
// Arguments : 1. Inode Number of file
//             2.Char *data to read into
//             3.Length of data
//             4.Offset to start reading from in file
//
// Returns: -1 -> For Failure
//		size -> For Success
//////////////////////////////////////////////////////////////////////
int read_i(int inumber, char *data, int length, int offset)
{
      int M, N, I, IB, R;
      char testc[length];
      uint32_t bitmap[1024];
      int total_files_read = 0;
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      int total_bytes;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int bitpos = inumber / 32768;
      int bitoffs = inumber % 32768;
      if (bitpos < 0 || inumber > I * 128)
      {
            printf("Failure in read_i because of wrong inode nunmber.\n");
            return -1;
      }
      return_val = read_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + bitpos, bitmap);
      if (return_val == -1)
      {
            printf("Failure in read_i because of inode bitmap read\n");
            return -1;
      }
      if (!TestBit(bitmap, bitoffs))
      {
            printf("Failure in read_i.\n");
            return -1;
      }
      int inode_block_no = inumber / 128;
      int inode_offs = inumber % 128;
      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in read_i.\n");
            return -1;
      }
      inode temp_store_node = inode_block_array[inode_offs];
      if (offset > temp_store_node.size)
      {
            printf("Failure in read_i.\n");
            return -1;
      }
      if (temp_store_node.valid == 0)
      {
            printf("Failure in read_i.\n");
            return -1;
      }
      int z1 = temp_store_node.size - offset - length;
      if (z1 < 0)
      {
            length = temp_store_node.size - offset;
      }
      int start_block = offset / (4096);
      int block_offset = offset % BLOCKSIZE;
      for (int iterator = start_block; iterator < 5; iterator++)
      {
            if (length <= 0)
                  break;
            int next_block = temp_store_node.direct[iterator];
            void *p2 = malloc(4096);
            total_bytes = BLOCKSIZE - block_offset;
            if (length < total_bytes)
            {
                  total_bytes = length;
            }
            return_val = read_block(stored_diskpointer, temp_super_block.data_block_idx + next_block, p2);
            if (return_val == -1)
            {
                  printf("Failure in read_i.\n");
                  return -1;
            }
            char *c;
            c = (char *)p2;
            for (int j = 0; j < total_bytes; j++)
            {
                  char z = c[j + block_offset];
                  testc[total_files_read++] = z;
            }
            length = length - total_bytes;
            block_offset = 0;
      }
      start_block = start_block - 5;
      if (start_block < 0)
            start_block = 0;
      if (length > 0)
      {
            uint32_t indirect[1024];
            if (read_block(stored_diskpointer, temp_store_node.indirect + temp_super_block.data_block_idx, indirect) < 0)
            {
                  printf("Failure in read_i.\n");
            }
            for (int iterator = start_block; iterator < 1024; iterator++)
            {
                  if (length <= 0)
                        break;
                  int next_block = indirect[iterator];
                  void *p2 = malloc(4096);
                  total_bytes = BLOCKSIZE - block_offset;
                  if (length < total_bytes)
                  {
                        total_bytes = length;
                  }
                  return_val = read_block(stored_diskpointer, temp_super_block.data_block_idx + next_block, p2);
                  if (return_val == -1)
                  {
                        printf("Failure in read_i.\n");
                        return -1;
                  }
                  // printf("%s  \n",c + block_offset);
                  char *c;
                  c = (char *)p2;
                  for (int j = 0; j < total_bytes; j++)
                  {
                        char z = c[j + block_offset];
                        testc[total_files_read++] = z;
                  }
                  //  printf("%s\n\n",testc);
                  // total_files_read += total_bytes;
                  length = length - total_bytes;
                  block_offset = 0;
            }
      }
      // printf("%s",testc);
      int safeindex = 0;
      while (safeindex <= strlen(testc))
      {
            data[safeindex] = testc[safeindex];
            safeindex++;
      }
      return total_files_read;
}
///////////////////////////////////////////////////////////////////////
// write_i function
// Arguments : 1. Inode Number of file
//             2.Char *data to read from
//             3.Length of data
//             4.Offset to start writing from in file
//
// Returns: -1 -> For Failure
//		size -> For Success
//////////////////////////////////////////////////////////////////////
int write_i(int inumber, char *data, int length, int offset)
{
      int M, N, I, IB, R;
      int bitmap[1024];
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int bitpos = inumber / 32768;
      int bitoffs = inumber % 32768;
      if (bitpos < 0 || inumber > I * 128)
      {
            printf("Failure in write_i because of wrong inode nunmber.\n");
            return -1;
      }
      return_val = read_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + bitpos, bitmap);
      if (return_val == -1)
      {
            printf("Failure in write_i because of inode bitmap read\n");
            return -1;
      }
      if (!TestBit(bitmap, bitoffs))
      {
            printf("Failure in write_i.\n");
            return -1;
      }
      int inode_block_no = inumber / 128;
      int inode_offs = inumber % 128;
      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in write_i.\n");
            return -1;
      }
      uint32_t indirect[1024];
      uint32_t tempindirect[1024];
      uint32_t tempdirect[5];
      inode temp_store_node = inode_block_array[inode_offs];
      if (temp_store_node.valid == 0)
      {
            printf("Failure in write_i.\n");
            return -1;
      }
      if (length > 1029 * BLOCKSIZE - offset)
      {
            length = 1029 * BLOCKSIZE - offset;
      }
      int req_num_blocks;
      int file_size_in_blocks = ceil((double)(temp_store_node.size) / BLOCKSIZE);
      int tempsize = temp_store_node.size;
      if (temp_store_node.size < length + offset)
      {
            // printf("debug %d\n",temp_store_node.size - offset);
            req_num_blocks = ceil((double)(length + offset) / BLOCKSIZE);
            temp_store_node.size = offset + length;
      }
      else
            req_num_blocks = ceil((double)(temp_store_node.size) / BLOCKSIZE);
      int *block_ids = emptyblocklist(req_num_blocks);
      if (block_ids == NULL)
      {
            printf("Failure in write_i.\n");
            return -1;
      }
      int iterator = 0;
      int idx = 0;
      // printf("%d\n\n",req_num_blocks);
      for (; idx < 5 && iterator < req_num_blocks; iterator++)
      {
            tempdirect[iterator] = temp_store_node.direct[iterator];
            if (iterator < file_size_in_blocks)
                  clearemptyblocks(tempdirect[iterator]);
            temp_store_node.direct[iterator] = block_ids[iterator];
            // printf("%d\n\n",block_ids[iterator]);
            idx++;
      }
      // for(int i=0;i<req_num_blocks;i++)
      //       printf("%d\n\n",block_ids[i]);
      if (iterator < req_num_blocks)
      {
            // printf("here\n");
            if (tempsize <= 5 * BLOCKSIZE)
            {
                  int *temp_ids = emptyblocklist(1);
                  temp_store_node.indirect = temp_ids[0];
            }
            else
            {
                  read_block(stored_diskpointer, temp_store_node.indirect + temp_super_block.data_block_idx, tempindirect);
            }
            int location = 0;
            for (; iterator < req_num_blocks; iterator++)
            {
                  if (location < file_size_in_blocks)
                        clearemptyblocks(tempindirect[location]);
                  indirect[location] = block_ids[iterator];
                  location++;
            }
            if (write_block(stored_diskpointer, temp_store_node.indirect + temp_super_block.data_block_idx, indirect) < 0)
            {
                  printf("Failure in write_i.\n");
                  return -1;
            }
      }
      int start_block = offset / (4096);
      int total_files_read = 0, block_offset = offset % BLOCKSIZE, total_bytes;
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (length <= 0 && req_num_blocks <= iterator)
                  break;
            int next_block = temp_store_node.direct[iterator];
            int prev_block = tempdirect[iterator];
            void *p2 = malloc(4096);
            total_bytes = BLOCKSIZE - block_offset;
            if (length < total_bytes)
            {
                  total_bytes = length;
            }
            return_val = read_block(stored_diskpointer, temp_super_block.data_block_idx + prev_block, p2);
            if (return_val == -1)
            {
                  printf("Failure in write_i.\n");
                  return -1;
            }
            char *c;
            c = (char *)p2;
            if (iterator >= start_block && length > 0)
            {
                  for (int j = 0; j < total_bytes; j++)
                  {
                        char z = data[total_files_read++];
                        c[j + block_offset] = z;
                  }
                  length = length - total_bytes;
                  block_offset = 0;
            }
            //  printf("%s\n\n",testc);
            // total_files_read += total_bytes;
            return_val = write_block(stored_diskpointer, temp_super_block.data_block_idx + next_block, c);
            if (return_val == -1)
            {
                  printf("Failure in write_i.\n");
                  return -1;
            }
      }
      start_block = start_block - 5;
      req_num_blocks = req_num_blocks - 5;
      if (read_block(stored_diskpointer, temp_store_node.indirect + temp_super_block.data_block_idx, indirect) < 0)
      {
            printf("Failure in write_i.\n");
            return -1;
      }
      for (int iterator = 0; iterator < 1024; iterator++)
      {
            if (length <= 0 && req_num_blocks <= iterator)
                  break;
            int next_block = indirect[iterator];
            int prev_block = tempindirect[iterator];
            void *p2 = malloc(4096);
            total_bytes = BLOCKSIZE - block_offset;
            if (length < total_bytes)
            {
                  total_bytes = length;
            }
            return_val = read_block(stored_diskpointer, temp_super_block.data_block_idx + prev_block, p2);
            if (return_val == -1)
            {
                  printf("Failure in write_i.\n");
                  return -1;
            }
            char *c;
            c = (char *)p2;
            if (iterator >= start_block && length > 0)
            {
                  // printf("%d\n",iterator);
                  for (int j = 0; j < total_bytes; j++)
                  {
                        char z = data[total_files_read++];
                        c[j + block_offset] = z;
                  }
                  // printf("%s\n\n\n",c);
                  length = length - total_bytes;
                  block_offset = 0;
            }
            //  printf("%s\n\n",testc);
            // total_files_read += total_bytes;
            return_val = write_block(stored_diskpointer, temp_super_block.data_block_idx + next_block, c);
            if (return_val == -1)
            {
                  printf("Failure in write_i.\n");
                  return -1;
            }
      }
      read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      inode_block_array[inode_offs] = temp_store_node;
      return_val = write_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in write_i.\n");
            return -1;
      }
      // void *p2 = malloc(4096);
      // return_val = read_block(stored_diskpointer, temp_super_block.data_block_idx + indirect[1], p2);
      // if (return_val == -1)
      // {
      //       printf("Failure in read_i.\n");
      //       return -1;
      // }
      // // printf("%s  \n",c + block_offset);
      // char *c;
      // c = (char *)p2;
      // printf("%s\n\n\n",c);
      return total_files_read;
}
///////////////////////////////////////////////////////////////////////
// fit_to_size function
// Arguments : 1. Inode Number of file
//             2. Size of file
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int fit_to_size(int inumber, int size)
{
      int M, N, I, IB, R;
      int bitmap[1024];
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));
      int bitpos = inumber / 32768;
      int bitoffs = inumber % 32768;
      if (bitpos < 0 || inumber > I * 128)
      {
            printf("Failure in fit_to_size.\n");
            return -1;
      }
      return_val = read_block(stored_diskpointer, temp_super_block.inode_bitmap_block_idx + bitpos, bitmap);
      if (return_val == -1)
      {
            printf("Failure in fit_to_size because of wrong inode bitmap read.\n");
            return -1;
      }
      if (!TestBit(bitmap, bitoffs))
      {
            printf("Failure in fit_to_size.\n");
            return -1;
      }
      int inode_block_no = inumber / 128;
      int inode_offs = inumber % 128;
      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in fit_to_size.\n");
            return -1;
      }
      inode temp_store_node = inode_block_array[inode_offs];
      if (temp_store_node.valid == 0)
      {
            printf("Failure in fit_to_size.\n");
            return -1;
      }
      if (size > temp_store_node.size)
      {
            return 0;
      }
      int cur_blocks = ceil((double)temp_store_node.size / BLOCKSIZE);
      int req_blocks = ceil((double)size / BLOCKSIZE);
      if (cur_blocks < 5)
      {
            while (req_blocks != cur_blocks)
            {
                  int datapos = temp_store_node.direct[cur_blocks - 1] / 32768;
                  int dataoffs = temp_store_node.direct[cur_blocks - 1] % 32768;
                  return_val = read_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                  if (return_val == -1)
                  {
                        printf("Failure in fit_to_size.\n");
                        return -1;
                  }
                  ClearBit(bitmap, dataoffs);
                  return_val = write_block(stored_diskpointer, temp_super_block.data_block_bitmap_idx + datapos, bitmap);
                  if (return_val == -1)
                  {
                        printf("Failure in fit_to_size.\n");
                        return -1;
                  }
                  cur_blocks--;
            }
      }
      temp_store_node.size = size;
      inode_block_array[inode_offs] = temp_store_node;
      return_val = write_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in fit_to_size.\n");
            return -1;
      }
      return 1;
}
///////////////////////////////////////////////////////////////////////
// create_dir function
// Arguments : 1. Directory path
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int create_dir(char *dirpath)
{
      // printf("%s      \n\n", dirpath);
      char dirpath1[3000];
      strcpy(dirpath1, dirpath);
      int M, N, I, IB, R;
      int bitmap[1024];
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      int total_files_read = 0;
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));

      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in create_dir function\n");
            return -1;
      }
      inode temp_store_node = inode_block_array[0];
      // if (temp_store_node.valid == 0)
      // {
      //       if (strcmp(dirpath1, "/") == 0)
      //       {
      //             temp_store_node.valid = 1;
      //       }
      //       else
      //       {
      //             printf("Failure in create_dir function\n");
      //             return -1;
      //       }
      // }

      int total_files = temp_store_node.size;
      int maxstore = BLOCKSIZE / sizeof(directory_struct);
      int tramp = maxstore * 5;
      // printf("%d\n", total_files);
      directory_struct temp_data_block1[maxstore];
      directory_struct files_in_directory[tramp + 1];
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (total_files_read < total_files)
            {
                  if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                  {
                        printf("Failure in create_dir function\n");
                        return -1;
                  }
                  for (int j = 0; j < maxstore; j++)
                  {
                        files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                  }
                  total_files_read += BLOCKSIZE / sizeof(directory_struct);
            }
            else
            {
                  break;
            }
      }
      // if (total_files > 0)
      // {
      //       printf("%s\n", files_in_directory[0].file_name);
      // }
      char *token;
      token = strtok(dirpath1, "/");
      if (token == NULL)
      {
            printf("Failure in create_dir function\n");
            return -1;
      }
      char *current_directory;
      current_directory = token;
      int inumber = 0;
      int inode_block_no = 0;
      int inode_offs = 0;
      while (token != NULL)
      {
            int file_found = 0;
            for (int iterator = 0; iterator < total_files; iterator++)
            {
                  // printf("1.%s       %s     %d\n\n", files_in_directory[0].file_name, token, total_files);
                  if (files_in_directory[iterator].valid && strcmp(files_in_directory[iterator].file_name, token) == 0)
                  {
                        // printf("Inside directory : %s %d\n", files_in_directory[iterator].file_name, files_in_directory[iterator].inumber);
                        // if (total_files > 1)
                        // {
                        //       printf("1.%s\n\n", files_in_directory[0].file_name);
                        // }
                        inumber = files_in_directory[iterator].inumber;

                        if (files_in_directory[iterator].type == 0)
                        {
                              printf("Failure in create_dir function\n");
                              return -1;
                        }

                        inode_block_no = inumber / 128;
                        inode_offs = inumber % 128;
                        // inode_block_array = (inode *)malloc(128 * sizeof(inode));
                        return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                        if (return_val == -1)
                        {
                              printf("Failure in create_dir function\n");
                              return -1;
                        }
                        temp_store_node = inode_block_array[inode_offs];
                        // printf("%d \n\n", temp_store_node.direct[0]);
                        if (temp_store_node.valid == 0)
                        {
                              printf("Failure in create_dir function. Invalid Path provided\n");
                              return -1;
                        }
                        total_files = temp_store_node.size;
                        // directory_struct files_in_directory[total_files + 1];
                        int total_files_read = 0;
                        for (int iterator = 0; iterator < 5; iterator++)
                        {
                              if (total_files_read < total_files)
                              {
                                    if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                                    {
                                          printf("Failure in create_dir function\n");
                                          return -1;
                                    }
                                    for (int j = 0; j < maxstore; j++)
                                    {
                                          files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                                    }

                                    total_files_read += BLOCKSIZE / sizeof(directory_struct);
                              }
                              else
                              {
                                    break;
                              }
                        }
                        file_found = 1;
                        // if (strcmp(token, "debug") == 0)
                        // {
                        //       printf("1.%s %d\n", files_in_directory[1].file_name, total_files);
                        // }
                        // printf("1.%s       %s     %d    %d\n\n", files_in_directory[0].file_name, token, total_files, file_found);
                        break;
                  }
            }
            if (file_found == 0)
            {
                  current_directory = token;
                  token = strtok(NULL, "/");
                  if (token != NULL)
                  {
                        printf("Error, Create Directory Failed, Parent Directory absent\n");
                        return -1;
                  }
                  int new_dir_inode = create_file();
                  int dir_entry_i;
                  return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                  // inode temp_store_node = inode_block_array[inode_offs];
                  // printf("%d \n\n", temp_store_node.valid);
                  // printf("debug %d\n", new_dir_inode);
                  for (dir_entry_i = 0; dir_entry_i < total_files; dir_entry_i++)
                  {
                        if (!files_in_directory[dir_entry_i].valid)
                        {
                              break;
                        }
                  }
                  if (dir_entry_i == total_files)
                        total_files++;
                  files_in_directory[dir_entry_i].valid = 1;
                  files_in_directory[dir_entry_i].inumber = new_dir_inode;
                  files_in_directory[dir_entry_i].type = 1;
                  strcpy(files_in_directory[dir_entry_i].file_name, current_directory);
                  if (20 < strlen(current_directory))
                        files_in_directory[dir_entry_i].length = 20;
                  else
                        files_in_directory[dir_entry_i].length = strlen(current_directory);
                  int total_written = 0;
                  for (int iterator = 0; iterator < 5; iterator++)
                  {
                        if (total_written < total_files)
                        {
                              for (int j = 0; j < maxstore; j++)
                              {
                                    // printf("%s %d\n\n", files_in_directory[j].file_name, j);
                                    temp_data_block1[j] = files_in_directory[iterator * maxstore + j];
                                    // if (total_files > 1)
                                    // {
                                    // printf("%s %d\n\n", files_in_directory[j].file_name, j);
                                    // }
                              }
                              total_written += BLOCKSIZE / sizeof(directory_struct);
                              if (temp_store_node.direct[iterator] == -1)
                              {
                                    int *block_ids = emptyblocklist(1);
                                    temp_store_node.direct[iterator] = block_ids[0];
                                    // printf("debug man %d\n\n", temp_store_node.direct[iterator]);
                              }
                              if (write_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                              {
                                    printf("Failure in create_dir function\n");
                                    return -1;
                              }
                        }
                        else
                        {
                              break;
                        }
                  }
                  temp_store_node.size++;
                  inode_block_array[inode_offs] = temp_store_node;
                  return_val = write_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                  if (return_val == -1)
                  {
                        printf("Failure in create_dir function writing bitmap of indoe  \n");
                        return -1;
                  }
                  printf("Create Directory Success, Name : %s\n", current_directory);
            }
            current_directory = token;
            token = strtok(NULL, "/");
      }
      // printf("1.%s       %s     %d    \n\n", files_in_directory[0].file_name, dirpath1, total_files);
      return 0;
}
///////////////////////////////////////////////////////////////////////
// write_file function
// Arguments : 1. File path
//             2.Char *data to read from
//             3.Length of data
//             4.Offset to start writing from in file
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int write_file(char *filepath, char *data, int length, int offset)
{
      char filepath1[3000];
      strcpy(filepath1, filepath);
      int M, N, I, IB, R;
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      int total_files_read = 0;
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));

      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in write_file function\n");
            return -1;
      }
      inode temp_store_node = inode_block_array[0];
      // if (temp_store_node.valid == 0)
      // {
      //       if (strcmp(filepath1, "/") == 0)
      //       {
      //             temp_store_node.valid = 1;
      //       }
      //       else
      //       {
      //             printf("Failure in write_file function\n");
      //             return -1;
      //       }
      // }

      int total_files = temp_store_node.size;
      int maxstore = BLOCKSIZE / sizeof(directory_struct);
      int tramp = maxstore * 5;
      // printf("%d\n", total_files);
      directory_struct temp_data_block1[maxstore];
      directory_struct files_in_directory[tramp + 1];
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (total_files_read < total_files)
            {
                  if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                  {
                        printf("Failure in write_file function\n");
                        return -1;
                  }
                  for (int j = 0; j < maxstore; j++)
                  {
                        files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                  }
                  total_files_read += BLOCKSIZE / sizeof(directory_struct);
            }
            else
            {
                  break;
            }
      }
      // if (total_files > 0)
      // {
      //       printf("%s\n", files_in_directory[0].file_name);
      // }
      char *token;
      token = strtok(filepath1, "/");
      if (token == NULL)
      {
            printf("Failure in write_file function\n");
            return -1;
      }
      char *current_directory;
      current_directory = token;
      int inumber = 0;
      int inode_block_no = 0;
      int inode_offs = 0;
      int itsafile = 0;
      int new_dir_inode = -1;
      while (token != NULL)
      {
            int file_found = 0;
            for (int iterator = 0; iterator < total_files; iterator++)
            {
                  if (files_in_directory[iterator].valid && strcmp(files_in_directory[iterator].file_name, token) == 0)
                  {
                        printf("Inside directory : %s\n", files_in_directory[iterator].file_name);
                        // if (total_files > 1)
                        // {
                        //       printf("1.%s\n\n", files_in_directory[0].file_name);
                        // }
                        inumber = files_in_directory[iterator].inumber;

                        inode_block_no = inumber / 128;
                        inode_offs = inumber % 128;
                        if (files_in_directory[iterator].type == 0)
                        {
                              itsafile = 1;
                              file_found = 0;
                              break;
                        }
                        // inode_block_array = (inode *)malloc(128 * sizeof(inode));
                        return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                        if (return_val == -1)
                        {
                              printf("Failure in write_file function\n");
                              return -1;
                        }
                        temp_store_node = inode_block_array[inode_offs];
                        // printf("%d \n\n", temp_store_node.direct[0]);
                        if (temp_store_node.valid == 0)
                        {
                              printf("File is empty.Not valid\n");
                              return -1;
                        }
                        total_files = temp_store_node.size;
                        // directory_struct files_in_directory[total_files + 1];
                        int total_files_read = 0;
                        for (int iterator = 0; iterator < 5; iterator++)
                        {
                              if (total_files_read < total_files)
                              {
                                    if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                                    {
                                          printf("Failure in write_file function\n");
                                          return -1;
                                    }
                                    for (int j = 0; j < maxstore; j++)
                                    {
                                          files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                                    }

                                    total_files_read += BLOCKSIZE / sizeof(directory_struct);
                              }
                              else
                              {
                                    break;
                              }
                        }
                        file_found = 1;
                        // if (strcmp(token, "debug") == 0)
                        // {
                        //       printf("1.%s %d\n", files_in_directory[1].file_name, total_files);
                        // }
                        break;
                  }
                  // if (itsafile == 1 && file_found == 1)
                  //       break;
            }
            if (file_found == 0)
            {
                  current_directory = token;
                  token = strtok(NULL, "/");
                  if (token != NULL)
                  {
                        printf("Error, Create File Failed, Parent Directory absent\n");
                        return -1;
                  }
                  if (itsafile == 0)
                  {
                        itsafile = 0;
                        new_dir_inode = create_file();
                        int dir_entry_i;
                        return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                        // inode temp_store_node = inode_block_array[inode_offs];
                        // printf("%d \n\n", temp_store_node.valid);
                        // printf("debug %d\n", new_dir_inode);
                        for (dir_entry_i = 0; dir_entry_i < total_files; dir_entry_i++)
                        {
                              if (!files_in_directory[dir_entry_i].valid)
                              {
                                    break;
                              }
                        }
                        if (dir_entry_i == total_files)
                              total_files++;
                        files_in_directory[dir_entry_i].valid = 1;
                        files_in_directory[dir_entry_i].inumber = new_dir_inode;
                        files_in_directory[dir_entry_i].type = 0;
                        strcpy(files_in_directory[dir_entry_i].file_name, current_directory);
                        if (20 < strlen(current_directory))
                              files_in_directory[dir_entry_i].length = 20;
                        else
                              files_in_directory[dir_entry_i].length = strlen(current_directory);
                        int total_written = 0;
                        for (int iterator = 0; iterator < 5; iterator++)
                        {
                              if (total_written < total_files)
                              {
                                    for (int j = 0; j < maxstore; j++)
                                    {
                                          // printf("%s %d\n\n", files_in_directory[j].file_name, j);
                                          temp_data_block1[j] = files_in_directory[iterator * maxstore + j];
                                          // if (total_files > 1)
                                          // {
                                          // printf("%s %d\n\n", files_in_directory[j].file_name, j);
                                          // }
                                    }
                                    total_written += BLOCKSIZE / sizeof(directory_struct);
                                    if (temp_store_node.direct[iterator] == -1)
                                    {
                                          int *block_ids = emptyblocklist(1);
                                          temp_store_node.direct[iterator] = block_ids[0];
                                          // printf("debug man %d\n\n", temp_store_node.direct[iterator]);
                                    }
                                    if (write_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                                    {
                                          printf("Failure in write_file function\n");
                                          return -1;
                                    }
                              }
                              else
                              {
                                    break;
                              }
                        }
                        temp_store_node.size++;
                        inode_block_array[inode_offs] = temp_store_node;
                        // read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                        return_val = write_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                        if (return_val == -1)
                        {
                              printf("Failure in write_file function\n");
                              return -1;
                        }
                        printf("Create File Success, Name : %s\n", current_directory);
                  }
                  else
                  {
                        new_dir_inode = inumber;
                        printf("Update File Success, Name : %s\n", current_directory);
                  }
            }
            current_directory = token;
            token = strtok(NULL, "/");
      }
      if (new_dir_inode != -1)
            write_i(new_dir_inode, data, length, offset);
      return 0;
}
///////////////////////////////////////////////////////////////////////
// create_dir function
// Arguments : 1. Directory path
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int remove_dir(char *dirpath)
{
      // printf("debug%s      \n\n", dirpath);
      char dirpath1[4096];
      strcpy(dirpath1, dirpath);
      int M, N, I, IB, R;
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));

      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in remove_dir.\n");
            return -1;
      }
      inode temp_store_node = inode_block_array[0];
      // if (temp_store_node.valid == 0)
      // {
      //       if (strcmp(dirpath1, "/") == 0)
      //       {
      //             temp_store_node.valid = 1;
      //       }
      //       else
      //       {
      //             printf("Failure in remove_dir.\n");
      //             return -1;
      //       }
      // }

      int total_files = temp_store_node.size;
      int maxstore = BLOCKSIZE / sizeof(directory_struct);
      int tramp = maxstore * 5;
      // printf("%d\n", total_files);
      directory_struct temp_data_block1[maxstore];
      directory_struct files_in_directory[tramp + 1];
      int total_files_read = 0;
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (total_files_read < total_files)
            {
                  if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                  {
                        printf("Failure in remove_dir.\n");
                        return -1;
                  }
                  for (int j = 0; j < maxstore; j++)
                  {
                        files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                  }
                  total_files_read += BLOCKSIZE / sizeof(directory_struct);
            }
            else
            {
                  break;
            }
      }
      // if (total_files > 0)
      // {
      //       printf("%s\n", files_in_directory[0].file_name);
      // }
      char *token;
      token = strtok(dirpath1, "/");
      if (token == NULL)
      {
            printf("Failure in remove_dir.\n");
            return -1;
      }
      char *current_directory;
      char dirname1[100];
      current_directory = token;
      int parent_inode, inumber = 0;
      int inode_block_no = 0;
      int inode_offs = 0;
      int file_found;
      while (token != NULL)
      {
            file_found = 0;
            for (int iterator = 0; iterator < total_files; iterator++)
            {
                  // printf("1.%s       %s     %d\n\n", files_in_directory[0].file_name, token, total_files);
                  if (files_in_directory[iterator].valid && strcmp(files_in_directory[iterator].file_name, token) == 0)
                  {
                        // printf("Inside directory : %s %d\n", files_in_directory[iterator].file_name, files_in_directory[iterator].inumber);
                        // if (total_files > 1)
                        // {
                        //       printf("1.%s\n\n", files_in_directory[0].file_name);
                        // }
                        parent_inode = inumber;
                        inumber = files_in_directory[iterator].inumber;

                        if (files_in_directory[iterator].type == 0)
                        {
                              printf("Failure in remove_dir.\n");
                              return -1;
                        }

                        inode_block_no = inumber / 128;
                        inode_offs = inumber % 128;
                        // inode_block_array = (inode *)malloc(128 * sizeof(inode));
                        return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                        if (return_val == -1)
                        {
                              printf("Failure in remove_dir.\n");
                              return -1;
                        }
                        temp_store_node = inode_block_array[inode_offs];
                        // printf("%d \n\n", temp_store_node.direct[0]);
                        if (temp_store_node.valid == 0)
                        {
                              printf("File is not valid.\n");
                              return -1;
                        }
                        total_files = temp_store_node.size;
                        // directory_struct files_in_directory[total_files + 1];
                        total_files_read = 0;
                        for (int iterator = 0; iterator < 5; iterator++)
                        {
                              if (total_files_read < total_files)
                              {
                                    if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                                    {
                                          printf("Failure in remove_dir.\n");
                                          return -1;
                                    }
                                    for (int j = 0; j < maxstore; j++)
                                    {
                                          files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                                    }

                                    total_files_read += BLOCKSIZE / sizeof(directory_struct);
                              }
                              else
                              {
                                    break;
                              }
                        }
                        file_found = 1;
                        break;
                  }
            }
            if (!file_found)
            {
                  printf("Error, Remove Directory Failed, Invalid Path\n");
                  return -1;
            }
            current_directory = token;
            token = strtok(NULL, "/");
      }
      printf("In Directory %s\n", current_directory);
      strcpy(dirname1, current_directory);
      for (int iterator = 0; iterator < total_files; iterator++)
      {
            // printf("1.%s       %s     %d\n\n", files_in_directory[0].file_name, token, total_files);
            strcpy(dirpath1, dirpath);
            strcat(dirpath1, "/");
            strcat(dirpath1, files_in_directory[iterator].file_name);
            // printf("%d\n", files_in_directory[iterator].inumber);
            if (files_in_directory[iterator].type == 1)
            {
                  printf("Removing Sub Directory %s\n", files_in_directory[iterator].file_name);
                  remove_dir(dirpath1);
            }

            else
            {
                  printf("Removing file %s\n", files_in_directory[iterator].file_name);
                  remove_file(files_in_directory[iterator].inumber);
            }
      }
      int total_written = 0;
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (total_written < total_files)
            {
                  for (int j = 0; j < maxstore; j++)
                  {
                        files_in_directory[iterator * maxstore + j].valid = 0;
                        temp_data_block1[j] = files_in_directory[iterator * maxstore + j];
                  }
                  total_written += BLOCKSIZE / sizeof(directory_struct);
                  if (write_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                  {
                        printf("Failure in remove_dir.\n");
                        return -1;
                  }
            }
            else
            {
                  break;
            }
      }
      remove_file(inumber);
      total_written = 0;
      inode_block_no = parent_inode / 128;
      inode_offs = parent_inode % 128;
      // inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in remove_dir.\n");
            return -1;
      }
      temp_store_node = inode_block_array[inode_offs];
      total_files = temp_store_node.size;
      // directory_struct files_in_directory[total_files + 1];
      total_files_read = 0;
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (total_files_read < total_files)
            {
                  if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                  {
                        printf("Failure in remove_dir.\n");
                        return -1;
                  }
                  for (int j = 0; j < maxstore; j++)
                  {
                        files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                  }

                  total_files_read += BLOCKSIZE / sizeof(directory_struct);
            }
            else
            {
                  break;
            }
      }
      int k = 0;
      int flag = 0;
      // token = strtok(dirname1, "/");
      // printf("entering %s\n", dirname1);
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (total_written < total_files)
            {
                  for (int j = 0; j < maxstore; j++)
                  {
                        // printf(" debug %s\n\n", files_in_directory[k].file_name);
                        k = iterator * maxstore + j;
                        if (strcmp(files_in_directory[k].file_name, dirname1) == 0)
                        {
                              flag = 1;
                        }
                        if (flag == 1)
                        {
                              if (k == total_files - 1)
                              {
                                    files_in_directory[k].valid = 0;
                                    continue;
                              }
                              files_in_directory[k] = files_in_directory[k + 1];
                        }
                        temp_data_block1[j] = files_in_directory[k];
                  }
                  total_written += BLOCKSIZE / sizeof(directory_struct);
                  if (write_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                  {
                        printf("Failure in remove_dir.\n");
                        return -1;
                  }
            }
            else
            {
                  break;
            }
      }
      temp_store_node.size--;
      inode_block_array[inode_offs] = temp_store_node;
      return_val = write_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in remove_dir.\n");
            return -1;
      }
      // printf("1.%s       %s     %d    \n\n", files_in_directory[0].file_name, dirpath1, total_files);
      return 0;
}
///////////////////////////////////////////////////////////////////////
// read_file function
// Arguments : 1. File path
//             2.Char *data to read from
//             3.Length of data
//             4.Offset to start writing from in file
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int read_file(char *filepath, char *data, int length, int offset)
{
      char filepath1[3000];
      strcpy(filepath1, filepath);
      int M, N, I, IB, R;
      N = stored_diskpointer->blocks;
      M = N - 1;
      I = floor(0.1 * M);
      IB = ceil((double)(I * 128) / (8 * BLOCKSIZE));
      R = M - I - IB;
      if (stored_diskpointer == NULL || Filesystem_State == Unmounted)
      {
            printf("Failure because disk is unmounted.\n");
            return -1;
      }
      int return_val;
      if (read_block(stored_diskpointer, 0, temp_data_block) < 0)
      {
            printf("Superblock Can not be accessed");
            return -1;
      }
      super_block temp_super_block;
      memcpy(&temp_super_block, temp_data_block, sizeof(super_block));

      inode *inode_block_array = (inode *)malloc(128 * sizeof(inode));
      return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx, inode_block_array);
      if (return_val == -1)
      {
            printf("Failure in read_file.\n");
            return -1;
      }
      inode temp_store_node = inode_block_array[0];

      if (temp_store_node.valid == 0)
      {
            if (strcmp(filepath1, "/") == 0)
            {
                  temp_store_node.valid = 1;
            }
            else
            {
                  printf("Failure in read_file.\n");
                  return -1;
            }
      }

      int total_files = temp_store_node.size;
      int maxstore = BLOCKSIZE / sizeof(directory_struct);
      int tramp = maxstore * 5;
      // printf("%d\n", total_files);
      directory_struct temp_data_block1[maxstore];
      directory_struct files_in_directory[tramp + 1];
      int total_files_read = 0;
      for (int iterator = 0; iterator < 5; iterator++)
      {
            if (total_files_read < total_files)
            {
                  if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                  {
                        printf("Failure in read_file.\n");
                        return -1;
                  }
                  for (int j = 0; j < maxstore; j++)
                  {
                        files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                  }
                  total_files_read += BLOCKSIZE / sizeof(directory_struct);
            }
            else
            {
                  break;
            }
      }
      // if (total_files > 0)
      // {
      //       printf("%s\n", files_in_directory[0].file_name);
      // }
      char *token;
      token = strtok(filepath1, "/");
      if (token == NULL)
      {
            printf("Failure in read_file.\n");
            return -1;
      }
      char *current_directory;
      current_directory = token;
      int inumber = 0;
      int inode_block_no = 0;
      int inode_offs = 0;
      int itsafile = 0;
      int new_dir_inode = -1;
      while (token != NULL)
      {
            int file_found = 0;

            for (int iterator = 0; iterator < total_files; iterator++)
            {
                  if (files_in_directory[iterator].valid && strcmp(files_in_directory[iterator].file_name, token) == 0)
                  {
                        // printf("Inside directory : %s\n", files_in_directory[iterator].file_name);
                        // if (total_files > 1)
                        // {
                        //       printf("1.%s\n\n", files_in_directory[0].file_name);
                        // }
                        inumber = files_in_directory[iterator].inumber;

                        inode_block_no = inumber / 128;
                        inode_offs = inumber % 128;
                        if (files_in_directory[iterator].type == 0)
                        {
                              itsafile = 1;
                              file_found = 0;
                              break;
                        }
                        // inode_block_array = (inode *)malloc(128 * sizeof(inode));
                        return_val = read_block(stored_diskpointer, temp_super_block.inode_block_idx + inode_block_no, inode_block_array);
                        if (return_val == -1)
                        {
                              printf("Failure in read_file.\n");
                              return -1;
                        }
                        temp_store_node = inode_block_array[inode_offs];
                        // printf("%d \n\n", temp_store_node.direct[0]);
                        if (temp_store_node.valid == 0)
                        {
                              printf("Empty File.Invalid\n");
                              return -1;
                        }
                        total_files = temp_store_node.size;
                        // directory_struct files_in_directory[total_files + 1];
                        int total_files_read = 0;
                        for (int iterator = 0; iterator < 5; iterator++)
                        {
                              if (total_files_read < total_files)
                              {
                                    if (read_block(stored_diskpointer, temp_super_block.data_block_idx + temp_store_node.direct[iterator], temp_data_block1) < 0)
                                    {
                                          printf("Failure in read_file.\n");
                                          return -1;
                                    }
                                    for (int j = 0; j < maxstore; j++)
                                    {
                                          files_in_directory[iterator * maxstore + j] = temp_data_block1[j];
                                    }

                                    total_files_read += BLOCKSIZE / sizeof(directory_struct);
                              }
                              else
                              {
                                    break;
                              }
                        }
                        file_found = 1;
                        // if (strcmp(token, "debug") == 0)
                        // {
                        //       printf("1.%s %d\n", files_in_directory[1].file_name, total_files);
                        // }
                        break;
                  }
                  // if (itsafile == 1 && file_found == 1)
                  //       break;
            }
            if (file_found == 0)
            {
                  current_directory = token;
                  token = strtok(NULL, "/");
                  if (token != NULL)
                  {
                        printf("Error, Read File Failed, Parent Directory absent\n");
                        return -1;
                  }
                  if (itsafile == 0)
                  {
                        printf("Read File Failed.Invalid Path\n");
                        return -1;
                  }
                  else
                  {
                        new_dir_inode = inumber;
                        read_i(new_dir_inode, data, length, offset);
                        printf("Read File Success, Name : %s\n", current_directory);
                  }
            }
            current_directory = token;
            token = strtok(NULL, "/");
      }
      return 0;
}
