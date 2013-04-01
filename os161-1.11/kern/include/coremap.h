#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <types.h>

struct lock;

enum page_state {
	FREE,
	DIRTY,
	CLEAN,
	FIXED
};

struct page {
	// the address space and virtual address to which the page is mapped
	struct addrspace *addrspace;
	vaddr_t addr;

	// the number of pages allocated with this one
	// this will be 0 if it's not the first page of an allocation
	unsigned long page_count;

	// the state of the page
	enum page_state state;
};

void coremap_bootstrap();

paddr_t coremap_getpages(unsigned long npages);
void coremap_freepages(paddr_t paddr);

extern struct page *coremap;

#endif
