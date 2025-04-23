/*
CSE 30341 Spring 2025 Flash Translation Assignment.
This is the main program for the simulator.
You should read and understand it, but don't change it.
*/

#include "disk.h"
#include "flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>

void do_sequential_write( struct disk *d, int nblocks );
void do_random_readwrite( struct disk *d, int nblocks, int ops );

int main( int argc, char *argv[] )
{
	if(argc!=4) {
		printf("use: %s <disk-blocks> <flash-pages> <pages-per-block>\n",argv[0]);
		return 1;
	}

	/* Parse the command line arguments */
	int disk_blocks = atoi(argv[1]);
	int flash_pages = atoi(argv[2]);
	int flash_pages_per_block = atoi(argv[3]);
	int total_ops = 10000;
	const char *filename = "myvirtualflash";

	/* Seed the random number generator, so as to vary results on each run. */
	srand(time(0));

	/* Create the underlying flash drive */
	printf("Creating flash drive %s with %d flash pages and %d flash blocks\n",filename,flash_pages,flash_pages/flash_pages_per_block);
	struct flash_drive *theflash = flash_create(filename,flash_pages,flash_pages_per_block);
	if(!theflash) {
		printf("couldn't open %s: %s\n",filename,strerror(errno));
		return 1;
	}

	/* Then create the flash translation layer around it. */
	printf("Creating flash translation layer...\n");
	struct disk *thedisk = disk_create(theflash,disk_blocks);

	/* Run the simulation */
	printf("Running %d I/O operations...\n",total_ops);
	do_sequential_write(thedisk,disk_blocks);
	do_random_readwrite(thedisk,disk_blocks,total_ops);

	/* Display the key output values. */
	printf("System Performance:\n");
	disk_report(thedisk);
	flash_report(theflash);
	
	flash_close(theflash);
	
	return 0;
}

/* Write to every block in the disk to get started. */

void do_sequential_write( struct disk *d, int nblocks )
{
	char data[DISK_BLOCK_SIZE];

	for(int i=0;i<nblocks;i++) {
		memset(data,i%127,sizeof(data));
		disk_write(d,i,data);
	}
}

/* Read and write randomly from the disk 80 / 20 percent. */

void do_random_readwrite( struct disk *d, int disk_blocks, int ops )
{
	char data[DISK_BLOCK_SIZE];

	int i;
	for(i=0;i<ops;i++) {
		int block = rand()%disk_blocks;
		if(rand()%10>=8) {
			memset(data,block%127,sizeof(data));
			disk_write(d,block,data);
		} else {
			disk_read(d,block,data);
			if(data[rand()%DISK_BLOCK_SIZE]!=(block%127)) {
				printf("ERROR: disk_read returned wrong block!\n");
				abort();
			}
		}
	}
}
 
