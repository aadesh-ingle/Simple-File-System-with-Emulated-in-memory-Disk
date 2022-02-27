#include "disk.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

///////////////////////////////////////////////////////////////////////
// read_block function
// Arguments : 1. Disk Pointer
// 		   2. Block Number to read
// 		   3.Void Pointer that we will copy the data into
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int read_block(disk *diskptr, int blocknr, void *block_data)
{
	// Check if blocks number is valid. If invalid return with failure
	if (blocknr <= -1 || blocknr + 1 > diskptr->blocks)
	{
		printf("Invalid Block Access\n");
		return -1;
	}
	// Create a temporary data block to copy contents of block from array of data
	char *temp_block;
	temp_block = (char *)malloc(BLOCKSIZE);
	temp_block = diskptr->block_arr[blocknr];

	// Using mem copy function
	// Copies "numBytes" bytes from address "from" to address "to"
	// memcpy(void *destination, const Source *from, size_t numBytes);
	memcpy(block_data, temp_block, BLOCKSIZE);

	// Check if memcpy successfull in block_data. If not return with failure
	if (block_data == NULL)
	{
		printf("Null Block Data accessed.Read Block Failed\n");
		return -1;
	}
	// Update NUmber of reads performed, Retrun with success.
	diskptr->reads = diskptr->reads + 1;
	return 0;
}
///////////////////////////////////////////////////////////////////////
// create_disk function
// Arguments : 1. Disk Pointer
// 		   2. Size of disk in No. of bytes
//
// Returns: Disk Pointer -> For Success
//		NULL -> For Failure
//////////////////////////////////////////////////////////////////////
disk *create_disk(disk *diskptr, int nbytes)
{
	// Calculate No. Of blocks that can be created
	// Blocksize is 4KB fixed
	// We subtract 24 from no. filse because that is required to save metadata of directory
	// initialise the directory
	int no_of_blocks = (int)((nbytes - 24) / BLOCKSIZE);
	diskptr->blocks = no_of_blocks;
	diskptr->writes = 0;
	diskptr->size = nbytes;
	diskptr->reads = 0;
	// Pointer to array of char* created
	// Each pointing to 1 block of data on disk
	diskptr->block_arr = (char **)malloc(no_of_blocks * sizeof(char *));
	if (diskptr->block_arr == NULL)
	{
		printf("Create Disk Failed.\n\n");
		return NULL;
	}
	// Run The iterator for number of blocks in the disk
	// Space for blocks is allocated
	for (int iterator = 0; iterator < no_of_blocks; iterator++)
	{
		diskptr->block_arr[iterator] = (char *)malloc(BLOCKSIZE);
		if (diskptr->block_arr[iterator] == NULL)
		{
			printf("Create Disk Failed\n\n");
			return NULL;
		}
	}
	// Print and return with success
	printf("Create Disk Successfull\nSize in No of Bytes : %d\nNo of Blocks : %d\n", nbytes, no_of_blocks);
	return diskptr;
}
///////////////////////////////////////////////////////////////////////
// write_block function
// Arguments : 1. Disk Pointer
// 		   2. Block Number to Write into
// 		   3.Void Pointer that we will copy the data from
//
// Returns: -1 -> For Failure
//		0 -> For Success
//////////////////////////////////////////////////////////////////////
int write_block(disk *diskptr, int blocknr, void *block_data)
{
	// Check if blocks number is valid. If invalid return with failure
	if (blocknr <= -1 || blocknr + 1 > diskptr->blocks)
	{
		printf("Write Block Failed.\n\n");
		return -1;
	}
	// Create a temporary data block to copy contents of block from array of data
	char *temp_block;
	temp_block = (char *)malloc(BLOCKSIZE);
	temp_block = (char *)block_data;
	// Using mem copy function
	// Copies "numBytes" bytes from address "from" to address "to"
	// memcpy(void *destination, const Source *from, size_t numBytes);
	memcpy(diskptr->block_arr[blocknr], temp_block, BLOCKSIZE);
	// Check if memcpy successfull in data block. If not return with failure
	if (diskptr->block_arr[blocknr] == NULL)
	{
		printf("Null Block Data accessed.Read Block Failed\n\n");
		return -1;
	}
	// Update NUmber of write operations performed, Retrun with success.
	diskptr->writes = diskptr->writes + 1;
	return 0;
}
///////////////////////////////////////////////////////////////////////
// free_disk function
//
// Arguments : 1. Disk Pointer
//
// Returns: -1 -> For Failure
//		0 -> For Success
//
//////////////////////////////////////////////////////////////////////
int free_disk(disk *diskptr)
{
	// Run The iterator for number of blocks in the disk
	// Free the space assigned to the blocks
	for (int iterator = 0; iterator < diskptr->blocks; iterator++)
	{
		free(diskptr->block_arr[iterator]);
	}
	// Free the disk
	free(diskptr->block_arr);
	// Print and return with success
	printf("Disk Free Succesfull\n");
	return 0;
}