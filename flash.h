/*
CSE 30341 Spring 2025 Flash Translation Assignment.
This is the interface to the flash drive.
Do not change this file.
*/

#ifndef FLASH_H
#define FLASH_H

#define FLASH_PAGE_SIZE 4096

/*
Create a new virtual flash drive in the file "filename", with the given number of pages.
Returns a pointer to a new flash drive object, or null on failure.
*/
struct flash_drive * flash_create( const char *filename, int flash_pages, int flash_pages_per_block );

/* Write exactly FLASH_PAGE_SIZE bytes to a given page on the device. */
void flash_write( struct flash_drive *d, int flash_page, const char *data );

/* Read exactly FLASH_PAGE_SIZE bytes from a given page on the device. */
void flash_read( struct flash_drive *d, int flash_page, char *data );

/* Erase an entire block of pages.  */
void flash_erase( struct flash_drive *d, int flash_block );

/* Return the number of pages in this device. */
int flash_npages( struct flash_drive *d );

/* Return the number of pages per block in this device. */
int flash_npages_per_block( struct flash_drive *d );

/* Print a report of total operations performed. */
void flash_report( struct flash_drive *d );

/* Close the virtual flash drive. */
void flash_close( struct flash_drive *d );

#endif
