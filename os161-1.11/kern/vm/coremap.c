#include <coremap.h>

#include <curthread.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <vm.h>

struct page *coremap;

// TODO are these required outside this file?

// a lock used while accessing the coremap
static struct lock *coremap_lock;

// nonzero if the coremap has been initialized
static int coremap_initialized = 0;
// the number of entries in the coremap
static unsigned int coremap_size;
// the number of non-free entries in the coremap (for debugging purposes)
static unsigned int coremap_pages_in_use = 0;

void coremap_bootstrap() {
	// get the size of the memory
	paddr_t first, last;
	ram_getsize(&first, &last);

	// calculate the size of memory (in pages)
	coremap_size = last / PAGE_SIZE;

	// manually allocate the coremap
	coremap = (struct page *) PADDR_TO_KVADDR(first);
	first += coremap_size * sizeof (struct page);

	// initialize the coremap
	unsigned int i;
	for (i = 0; i < first / PAGE_SIZE; i++) {
		coremap[i].addr = (vaddr_t) NULL;
		coremap[i].addrspace = (struct addrspace *) NULL;
		coremap[i].page_count = 0;
		coremap[i].state = FIXED;

		coremap_pages_in_use += 1;
	}
	for (i = first / PAGE_SIZE; i < coremap_size; i++) {
		coremap[i].addr = (vaddr_t) NULL;
		coremap[i].addrspace = (struct addrspace *) NULL;
		coremap[i].page_count = 0;
		coremap[i].state = FREE;
	}

	// create a lock for the coremap
	coremap_lock = lock_create("coremap_lock");
	if (coremap_lock == NULL) panic("Unable to instantiate coremap_lock");

	DEBUG(DB_COREMAP, "%u of %u pages in use after coremap initialization.\n", coremap_pages_in_use, coremap_size);

	coremap_initialized = 1;
}

paddr_t coremap_getpages(unsigned long npages) {
	if (npages < 1) {
		panic("Attempting to get %lu pages from the coremap.\n", npages);
	}

	paddr_t paddr;

	if (coremap_initialized) {
		lock_acquire(coremap_lock);

		// find npages consecutive pages in physical memory
		unsigned int i, pages = 0;
		for (i = 0; i < coremap_size; i++) {
			if (coremap[i].state == FREE) {
				pages += 1;
			} else {
				pages = 0;
			}

			if (pages == npages) {
				break;
			}
		}

		if (i < coremap_size) {
			// we found space
			unsigned int page = i - (npages - 1);

			// TODO are these being set correctly?
			coremap[page].addr = (vaddr_t) NULL;
			coremap[page].addrspace = (struct addrspace *) NULL;
			coremap[page].page_count = npages;
			coremap[page].state = DIRTY;

			for (i = 1; i < npages; i++) {
				coremap[page + i].addr = (vaddr_t) NULL;
				coremap[page + i].addrspace = (struct addrspace *) NULL;
				coremap[page + i].page_count = 0;
				coremap[page + i].state = DIRTY;
			}

			coremap_pages_in_use += npages;

			DEBUG(DB_COREMAP, "%u of %u pages in use after allocation of %lu pages.\n", coremap_pages_in_use, coremap_size, npages);

			paddr = page * PAGE_SIZE;
		} else {
			// we didn't find space
			paddr = (paddr_t) NULL;
		}

		lock_release(coremap_lock);
	} else {
		// just take some ram (which won't be swappable once the coremap is initialized)
		DEBUG(DB_COREMAP, "Allocating %lu pages before coremap initialized.\n", npages);

		paddr = ram_stealmem(npages);
	}

	return paddr;
}

void coremap_freepages(paddr_t paddr) {
	if (coremap_initialized) {
		unsigned long page = paddr / PAGE_SIZE;

		if (paddr % PAGE_SIZE != 0) {
			DEBUG(DB_COREMAP, "Attempting free in the middle of a page. This might not matter though.\n");
		}

		lock_acquire(coremap_lock);

		unsigned long npages = coremap[page].page_count;
		if (npages != 0) {
			// we're at the start of an allocation block so we can free the pages in the block
			unsigned int i;
			for (i = 0; i < npages; i++) {
				if (coremap[page + i].state == DIRTY || coremap[page + i].state == CLEAN) {
					coremap[page + i].addr = (vaddr_t) NULL;
					coremap[page + i].addrspace = (struct addrspace *) NULL;
					coremap[page + i].page_count = 0;
					coremap[page + i].state = FREE;
				} else if (coremap[page + i].state == FIXED) {
					DEBUG(DB_COREMAP, "Attempting free on a fixed page.\n");
				} else {
					DEBUG(DB_COREMAP, "Attempting free on an unallocated page.\n");
				}
			}

			coremap_pages_in_use -= npages;

			DEBUG(DB_COREMAP, "%u of %u pages in use after free of %lu pages.\n", coremap_pages_in_use, coremap_size, npages);
		} else {
			DEBUG(DB_COREMAP, "Attemping free in the middle of an allocation block.\n");
		}

		lock_release(coremap_lock);
	} else {
		panic("Trying to free 0x%x before the coremap is initialized.", paddr);
	}
}
