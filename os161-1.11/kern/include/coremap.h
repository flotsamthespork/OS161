#ifndef _COREMAP_H_
#define _COREMAP_H_

#include <types.h>

struct lock;

enum page_state {
	FREE,
	ALLOCATED,
	FIXED
};

struct coremap_page {
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
void coremap_shutdown();

paddr_t coremap_getpages(unsigned long npages);
void coremap_freepages(paddr_t paddr);

// Get and set whether a page is swappable or not
int coremap_ispagefixed(paddr_t paddr);
void coremap_setpagefixed(paddr_t paddr, int fixed);

// Get and set the addrspace and vaddr of a page
void coremap_getpagevaddr(paddr_t paddr, struct addrspace **addrspace, vaddr_t *vaddr);
void coremap_setpagevaddr(paddr_t paddr, struct addrspace *addrspace, vaddr_t vaddr);

#endif
