/*
CSE 30341 Spring 2025 Flash Translation Assignment.
This is the flash translation layer.
You should write all your code here.
*/

#include "disk.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

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
	// Allocate memory for the disk structure
    struct disk *d = malloc(sizeof(*d));
    if (d == NULL) {
        fprintf(stderr, "Memory allocation failed for disk structure\n");
        return NULL;
    }

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
    
    // page metadata: no reverse mapping, all pages free
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
    printf("  [Mapping] disk_block %d -> flash_page %d\n", disk_block, flash_page);
    
    if (flash_page < 0) {
        //block has never been written, return zeros
        printf("  [Info] Block %d has not been written yet. Returning zeros.\n", disk_block);
        memset(data, 0, DISK_BLOCK_SIZE);
    } else {
        int status = d->page_status[flash_page];
        int mapped_block = d->page_to_block[flash_page];

        // Sanity check: verify page is valid and points to correct block
        if (status != PAGE_VALID || mapped_block != disk_block) {
            fprintf(stderr, "  ERROR: Invalid mapping! flash_page %d has status=%d, maps to disk block %d (expected %d)\n",
                    flash_page, status, mapped_block, disk_block);
            memset(data, 0, DISK_BLOCK_SIZE);  // Fallback
            return -1;
        }
        // read the data from the mapped flash page
        printf("  [Action] Reading from flash page %d\n", flash_page);
        flash_read(d->flash_drive, flash_page, data);
        printf("  [Debug] First 8 bytes of read data: ");
        for (int i = 0; i < 8; i++) {
            printf("%02x ", (unsigned char)data[i]);
        }
        printf("\n");
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
    
    //find a free page to write the data
    int new_page = find_free_page(d, -1);
    printf("  [Find] Initial free page search result: %d\n", new_page);

    // GC if needed
    if (new_page < 0) {
        int block_to_clean = select_block_to_clean(d);
        if (block_to_clean >= 0) {
            printf("  [GC] Cleaning block %d\n", block_to_clean);
            clean_block(d, block_to_clean);
            new_page = find_free_page(d, block_to_clean);
        }
    }

    // Wear-leveling fallback
    if (new_page < 0) {
        int min_block = 0, min_count = d->erase_count[0];
        for (int i = 1; i < d->flash_blocks; i++) {
            if (d->erase_count[i] < min_count) {
                min_count = d->erase_count[i];
                min_block = i;
            }
        }
        printf("  [WearLeveling] Cleaning block %d with lowest erase count %d\n", min_block, min_count);
        clean_block(d, min_block);
        new_page = find_free_page(d, min_block);
    }
    
    if (new_page < 0) {
        // fprintf(stderr, "  ERROR: No free page available even after GC!\n");
        return -1;
    }

    int old_page = d->block_to_page[disk_block];
    if (old_page >= 0) {
        d->page_status[old_page] = PAGE_INVALID;
        d->page_to_block[old_page] = -1;
    }

    // Step 2: write new data
    flash_write(d->flash_drive, new_page, data);
    printf("  [Write] Writing data to flash page %d for disk_block %d\n", new_page, disk_block);



    // Step 3: update mapping
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
    // char buffer[DISK_BLOCK_SIZE];
    printf("\n[clean_block] Cleaning block %d\n", block_num);
    
    // Step 1: Record valid page info before erase
    struct {
        int page_num;
        int disk_block;
        char data[DISK_BLOCK_SIZE];  
    } valid_pages[d->pages_per_block];
    int valid_count = 0;

    for (int p = 0; p < d->pages_per_block; p++) {
        int page_num = block_start + p;

        if (d->page_status[page_num] == PAGE_VALID) {
            int disk_block = d->page_to_block[page_num];
            if (disk_block >= 0) {
                flash_read(d->flash_drive, page_num, valid_pages[valid_count].data); 
                valid_pages[valid_count].page_num = page_num;
                valid_pages[valid_count].disk_block = disk_block;
                valid_count++;
                printf("  [Migrate] Valid page %d still mapped to disk block %d\n", page_num, disk_block);
            }
        }
    }

    // Step 2: Erase the block first
    for (int p = 0; p < d->pages_per_block; p++) {
        int page_num = block_start + p;
        d->page_status[page_num] = PAGE_INVALID;
        d->page_to_block[page_num] = -1;
    }
    flash_erase(d->flash_drive, block_num);
    d->erase_count[block_num]++;
    printf("  [Erase] Block %d erased (erase count now %d)\n", block_num, d->erase_count[block_num]);


    for (int p = 0; p < d->pages_per_block; p++) {
        int page_num = block_start + p;
        d->page_status[page_num] = PAGE_FREE;
        d->page_to_block[page_num] = -1;
    }

    // Step 3: Migrate valid pages to now-free pages in this block or others
    for (int i = 0; i < valid_count; i++) {
        int old_page = valid_pages[i].page_num;
        int disk_block = valid_pages[i].disk_block;

        int new_page = find_free_page(d, -1); // Allow using this block now
        if (new_page >= 0) {
            flash_write(d->flash_drive, new_page, valid_pages[i].data);

            d->block_to_page[disk_block] = new_page;
            d->page_to_block[new_page] = disk_block;
            d->page_status[new_page] = PAGE_VALID;

            printf("  [Remap] disk_block %d moved from old page %d to new page %d\n",
                disk_block, old_page, new_page);
            
        } else {
            fprintf(stderr, "  ERROR: No free page available during cleaning (post-erase)!\n");
        }
    }
}



// find a free page for writing
int find_free_page(struct disk *d, int avoid_block) {
    if (d->flash_blocks == 0 || d->pages_per_block == 0) {
        fprintf(stderr, "ERROR: Invalid disk configuration (0 blocks or pages).\n");
        return -1;
    }

    int best_page = -1;
    int lowest_erase = INT_MAX;

    // Iterate over all flash blocks
    for (int b = 0; b < d->flash_blocks; b++) {
        if (b == avoid_block) continue;  // Skip the block to avoid

        int block_start = b * d->pages_per_block;

        // Iterate over all pages in the block
        for (int p = 0; p < d->pages_per_block; p++) {
            int page = block_start + p;

            // Check if the page is free
            if (d->page_status[page] == PAGE_FREE) {
                // Track the block with the lowest erase count
                if (d->erase_count[b] < lowest_erase) {
                    lowest_erase = d->erase_count[b];
                    best_page = page;
                }
            }
        }
    }

    return best_page;  // -1 if no free pages found
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
