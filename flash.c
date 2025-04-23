/*
CSE 30341 Spring 2025 Flash Translation Assignment.
This is the implementation of the flash drive.
Do not change this file.
*/

#define _XOPEN_SOURCE 500L

#include "flash.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#define ABS(x) ( (x)<(0) ? -(x) : (x) )

struct flash_drive {
	int fd;
	int npages;
	int page_size;
	int npages_per_block;
	int nreads;
	int nwrites;
	int nerases;
	int threads_inside;
	int *page_status;
	int *page_writes;
};

struct flash_drive * flash_create( const char *flashname, int npages, int npages_per_block )
{
	struct flash_drive *d;

	d = malloc(sizeof(*d));
	if(!d) return 0;

	d->fd = open(flashname,O_CREAT|O_RDWR,0777);
	if(d->fd<0) {
		free(d);
		return 0;
	}

	d->npages = npages;
	d->npages_per_block = npages_per_block;
	d->page_size = FLASH_PAGE_SIZE;
	d->threads_inside = 0;
	d->nreads = 0;
	d->nwrites = 0;
	d->page_status = calloc(1,sizeof(int)*d->npages);
	d->page_writes = calloc(1,sizeof(int)*d->npages);
	
	if(ftruncate(d->fd,d->npages*d->page_size)<0) {
		close(d->fd);
		free(d);
		return 0;
	}

	return d;
}

void flash_write( struct flash_drive *d, int page, const char *data )
{
	d->threads_inside++;
	
	if(d->threads_inside>1) {
		fprintf(stderr,"flash_write: CRASH: multiple threads in flash drive at once!\n");
		abort();
	}
	
	if(page<0 || page>=d->npages) {
		fprintf(stderr,"flash_write: CRASH: invalid page #%d\n",page);
		abort();
	}

	if(d->page_status[page]) {
		fprintf(stderr,"flash_write: CRASH: page #%d written twice without erasing first.\n",page);
		abort();
	}
	
	printf("flash_write: page %d\n",page);

	usleep(200);
	
	int actual = pwrite(d->fd,(char*)data,d->page_size,page*d->page_size);
	if(actual!=d->page_size) {
		fprintf(stderr,"flash_write: CRASH: failed to write page #%d: %s\n",page,strerror(errno));
		abort();
	}

	d->page_status[page] = 1;
	d->page_writes[page]++;
	d->threads_inside--;
	d->nwrites++;
}

void flash_erase( struct flash_drive *d, int block )
{
	d->threads_inside++;

	int page = block * d->npages_per_block;
	
	if(d->threads_inside>1) {
		fprintf(stderr,"flash_erase: CRASH: multiple threads in flash drive at once!\n");
		abort();
	}
	
	if(page<0 || page>=d->npages) {
		fprintf(stderr,"flash_write: CRASH: invalid page #%d\n",page);
		abort();
	}

	if(page % d->npages_per_block!=0) {
		fprintf(stderr,"flash_write: CRASH: invalid starting page (%d) for erase operation",page);
		abort();
	}
	
	printf("flash_erase: page %d through %d\n",page,page+d->npages_per_block-1);

	usleep(500);

	int block_length = d->page_size * d->npages_per_block;
	
	char *data = calloc(1,block_length);
	
	int actual = pwrite(d->fd,(char*)data,block_length,page*d->page_size);
	if(actual!=block_length) {
		fprintf(stderr,"flash_write: CRASH: failed to erase page #%d: %s\n",page,strerror(errno));
		abort();
	}

	free(data);

	for(int i=0;i<d->npages_per_block;i++) {
		d->page_status[page+i] = 0;
	}
	
	d->threads_inside--;
	d->nerases++;
}

void flash_read( struct flash_drive *d, int page, char *data )
{
	d->threads_inside++;

	if(d->threads_inside>1) {
		fprintf(stderr,"flash_read: CRASH: multiple threads in flash drive at once!\n");
		abort();
	}
	
	if(page<0 || page>=d->npages) {
		fprintf(stderr,"flash_read: CRASH: invalid page #%d\n",page);
		abort();
	}

	printf("flash_read: page %d\n",page);
	
	usleep(50);
	
	int actual = pread(d->fd,(char*)data,d->page_size,page*d->page_size);
	if(actual!=d->page_size) {
		fprintf(stderr,"flash_read: CRASH: failed to read page #%d: %s\n",page,strerror(errno));
		abort();
	}

	d->threads_inside--;
	d->nreads++;
}

int flash_npages( struct flash_drive *d )
{
	return d->npages;
}

int flash_npages_per_block( struct flash_drive *d )
{
	return d->npages_per_block;
}

void flash_report( struct flash_drive *d )
{
	printf("\tflash  reads: %d\n",d->nreads);
	printf("\tflash writes: %d\n",d->nwrites);
	printf("\tflash erases: %d\n",d->nerases);

	int max_page = 0;
	int max_writes = d->page_writes[0];

	int min_page = 0;
	int min_writes = max_writes;
	
	for(int i=1;i<d->npages;i++) {
		if(d->page_writes[i]>max_writes) {
			max_page = i;
			max_writes = d->page_writes[i];
		}
		if(d->page_writes[i]<min_writes) {
			min_page = i;
			min_writes = d->page_writes[i];
		}
	}

	printf("\twear differential:\n");
	printf("\tmost written:  page %d was written %d times\n",max_page,max_writes);
	printf("\tleast written: page %d was written %d times\n",min_page,min_writes);

		
	printf("\tratio of most/least: ");
	if(min_writes==0) {
		printf("infinite!\n");
	} else {
		printf("%.2lf\n",(double)max_writes/min_writes);
	}		
}

void flash_close( struct flash_drive *d )
{
	free(d->page_status);
	close(d->fd);
	free(d);
}
