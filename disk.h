/*
CSE 30341 Spring 2025 Flash Translation Assignment.
This is the interface to the simulated disk.
You should read and understand it, but don't change it.
*/

#ifndef DISK_H
#define DISK_H

#include "flash.h"

#define DISK_BLOCK_SIZE 4096

/* Create a new flash translation layer on top of flash drive f, simulating # disk_blocks */
struct disk * disk_create( struct flash_drive *f, int disk_blocks );

/* Read exactly DISK_BLOCK_SIZE bytes from the given disk block */
int  disk_read( struct disk *d, int disk_block, char *data );

/* Write exactly DISK_BLOCK_SIZE bytes to the given disk block */
int  disk_write( struct disk *d, int disk_block, const char *data );

/* Report the total number of operations done on the disk. */
void disk_report( struct disk *d );

/* Close the flash translation layer. */
void disk_close( struct disk *d );

#endif
