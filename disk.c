/*
CSE 30341 Spring 2025 Flash Translation Assignment.
This is the flash translation layer.
You should write all your code here.
*/

#include "disk.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//possible states for a flash page
#define PAGE_FREE 0
#define PAGE_VALID 1
#define PAGE_INVALID 2

/*
Structure of the flash translation layer.
Go ahead and add or change things here as needed.
*/

struct disk {
	struct flash_drive *flash_drive;
	int disk_blocks;//number of logical disk blocks
	int flash_pages;//number of flash pages
	int pages_per_block;//number of pages in each flash block
	int flash_blocks;//number of flash blocks

	int *block_to_page; //maps disk blocks to flash pages
    int *page_to_block;   //reverse mapping - flash pages to disk blocks
	int *page_status; //status of each flash page
	int *erase_count; //count of erases for each flash block

	int next_free_page; //next free page for writing
	int nreads;
	int nwrites;
};

int find_free_page(struct disk *d, int preferred_block);
int select_block_to_clean(struct disk *d);
void clean_block(struct disk *d, int block_num);

/*
Create a new flash translation layer for this flash drive f, and simulated number of blocks
Go ahead and add or change things here as needed.
*/

struct disk * disk_create( struct flash_drive *f, int disk_blocks )
{
	struct disk *d = malloc(sizeof(*d));
	d->flash_drive = f;
    d->disk_blocks = disk_blocks;
    d->flash_pages = flash_npages(f);
    d->pages_per_block = flash_npages_per_block(f);
    d->flash_blocks = d->flash_pages / d->pages_per_block;
    
    // init mapping tables
    d->block_to_page = malloc(sizeof(int) * disk_blocks);
    d->page_to_block = malloc(sizeof(int) * d->flash_pages);
    d->page_status = malloc(sizeof(int) * d->flash_pages);
    d->erase_count = malloc(sizeof(int) * d->flash_blocks);
    
    // init all mappings and states
    for (int i = 0; i < disk_blocks; i++) {
        d->block_to_page[i] = -1;  // no mapping initially
    }
    
    for (int i = 0; i < d->flash_pages; i++) {
        d->page_to_block[i] = -1;  // no reverse mapping initially
        d->page_status[i] = PAGE_FREE;
    }
    
    for (int i = 0; i < d->flash_blocks; i++) {
        d->erase_count[i] = 0;
    }
    
	d->nreads = 0;
	d->nwrites = 0;
	return d;
}

/*
Read a disk block through the flash translation layer.
Go ahead and add or change things here as needed.
*/

int disk_read( struct disk *d, int disk_block, char *data )
{
	printf("disk_read: block %d\n", disk_block);
	if (disk_block < 0 || disk_block >= d->disk_blocks) {
        fprintf(stderr, "disk_read: invalid block number %d\n", disk_block);
        return -1;
    }
    
    int flash_page = d->block_to_page[disk_block];
    
    if (flash_page < 0) {
        //block has never been written, return zeros
        memset(data, 0, DISK_BLOCK_SIZE);
    } else {
        // read the data from the mapped flash page
        flash_read(d->flash_drive, flash_page, data);
    }

	d->nreads++;
	return 0;
}

/*
Write a disk block through the flash translation layer.
Go ahead and add or change things here as needed.
*/

int disk_write( struct disk *d, int disk_block, const char *data )
{
	printf("disk_write: block %d\n",disk_block);

	if (disk_block < 0 || disk_block >= d->disk_blocks) {
        fprintf(stderr, "disk_write: invalid block number %d\n", disk_block);
        return -1;
    }
    
    // if there's an existing mapping, invalidate the old page
    int old_page = d->block_to_page[disk_block];
    if (old_page >= 0) {
        d->page_status[old_page] = PAGE_INVALID;
        d->page_to_block[old_page] = -1; // reverse mapping
    }
    
    //find a free page to write the data
    int new_page = find_free_page(d, -1);
    
    // if no free pages, we need to clean (garbage collect) a block
    if (new_page < 0) {
        int block_to_clean = select_block_to_clean(d);
        if (block_to_clean >= 0) {
            clean_block(d, block_to_clean);
            //try to find a free page again
            new_page = find_free_page(d, block_to_clean);
        }
        
        // if still no free pages, we need to clean more blocks
        if (new_page < 0) {
            // choose the block with lowest erase count for wear leveling
            int min_count = d->erase_count[0];
            int min_block = 0;
            
            for (int i = 1; i < d->flash_blocks; i++) {
                if (d->erase_count[i] < min_count) {
                    min_count = d->erase_count[i];
                    min_block = i;
                }
            }
            
            clean_block(d, min_block);
            new_page = find_free_page(d, min_block);
        }
    }
    
    // write the data to the new page
    flash_write(d->flash_drive, new_page, data);
    
    //update the mapping
    d->block_to_page[disk_block] = new_page;
    d->page_to_block[new_page] = disk_block;
    d->page_status[new_page] = PAGE_VALID;

	d->nwrites++;
	return 0;
}

/*
Report the total number of operations performed.
You can add more if you like here, but keep the display of reads and writes.
*/

void disk_report( struct disk *d )
{
	printf("\tdisk reads: %d\n",d->nreads);
	printf("\tdisk writes: %d\n",d->nwrites);

	//free alloc mem

	free(d->block_to_page);
    free(d->page_to_block);
    free(d->page_status);
    free(d->erase_count);
    free(d);
}

//clean a blk by moving valid pages and erasing
void clean_block(struct disk *d, int block_num) {
    int block_start = block_num * d->pages_per_block;
    char buffer[DISK_BLOCK_SIZE];
    
    // mv valid pages to new locations
    for (int p = 0; p < d->pages_per_block; p++) {
        int page_num = block_start + p;
        
        if (d->page_status[page_num] == PAGE_VALID) {
            // find the disk block that maps to this page
            int disk_block = d->page_to_block[page_num];
            
            if (disk_block >= 0) {
                // only migrate if this page is still the current one
                if (d->block_to_page[disk_block] != page_num) continue;
            
                flash_read(d->flash_drive, page_num, buffer);
                
                int new_page = find_free_page(d, block_num);
                if (new_page >= 0) {
                    flash_write(d->flash_drive, new_page, buffer);
                    
                    // updaye mappings
                    d->block_to_page[disk_block] = new_page;
                    d->page_to_block[new_page] = disk_block;
                    d->page_status[new_page] = PAGE_VALID;
                }
            }
        }
    }
    
    //erase the block
    flash_erase(d->flash_drive, block_num);
    d->erase_count[block_num]++;
    
    // mark all pages in this block as free
    for (int p = 0; p < d->pages_per_block; p++) {
        int page_num = block_start + p;
        d->page_status[page_num] = PAGE_FREE;
        d->page_to_block[page_num] = -1; // reverse mapping
    }
}

// find a free page for writing
int find_free_page(struct disk *d, int avoid_block) {
    // first try to find a block with the lowest erase count
    int lowest_erase = d->erase_count[0];
    int best_block = 0;
    
    for (int i = 1; i < d->flash_blocks; i++) {
        // skip the block were trying to avoid
        if (i == avoid_block) continue;
        
        if (d->erase_count[i] < lowest_erase) {
            lowest_erase = d->erase_count[i];
            best_block = i;
        }
    }
    
    // try block with lowest erase count first
    int start_page = best_block * d->pages_per_block;
    for (int i = 0; i < d->pages_per_block; i++) {
        if (d->page_status[start_page + i] == PAGE_FREE) {
            return start_page + i;
        }
    }
    
    // if block full, check all blocks
    for (int b = 0; b < d->flash_blocks; b++) {
        // skip the avoided block and the one we already checked
        if (b == avoid_block || b == best_block) continue;
        
        int block_start = b * d->pages_per_block;
        for (int p = 0; p < d->pages_per_block; p++) {
            if (d->page_status[block_start + p] == PAGE_FREE) {
                return block_start + p;
            }
        }
    }
    
    // no free pages 
    return -1;
}

//find blk to clean
int select_block_to_clean(struct disk *d) {
    int best_block = -1;
    int max_invalid = -1;
    
    // find block with most invalid pages
    for (int b = 0; b < d->flash_blocks; b++) {
        int invalid_count = 0;
        int block_start = b * d->pages_per_block;
        
        for (int p = 0; p < d->pages_per_block; p++) {
            if (d->page_status[block_start + p] == PAGE_INVALID) {
                invalid_count++;
            }
        }
        
        // choose blk with most invalid pages
        if (invalid_count > max_invalid) {
            max_invalid = invalid_count;
            best_block = b;
        }
    }
    
    // only clean if at least one page is invalid
    if (max_invalid > 0) {
        return best_block;
    }
    
    return -1; // dont clean any block yet
}