#include <coremap.h>

#include <addrspace.h>
#include <curthread.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <vm.h>

struct coremap_page *coremap;

// a lock used while accessing the coremap
static struct lock *coremap_lock;

// nonzero if the coremap has been initialized
static int coremap_initialized = 0;
// the number of entries in the coremap
static unsigned int coremap_size;
// the number of non-free entries in the coremap (for debugging purposes)
static unsigned int coremap_pages_in_use = 0;

static unsigned int last_page_used = 0;

void coremap_bootstrap() {
	// get the size of the memory
	paddr_t first, last;
	ram_getsize(&first, &last);

	// calculate the size of memory (in pages)
	coremap_size = last / PAGE_SIZE;

	// manually allocate the coremap
	coremap = (struct coremap_page *) PADDR_TO_KVADDR(first);
	first += coremap_size * sizeof (struct coremap_page);

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
	if (coremap_lock == NULL) panic("Unable to instantiate coremap_lock.\n");

	DEBUG(DB_COREMAP, "%u of %u pages in use after coremap initialization.\n", coremap_pages_in_use, coremap_size);

	coremap_initialized = 1;
}

void coremap_shutdown() {
	lock_destroy(coremap_lock);
}

/** Utility method to get a free page in the swapfile. **/
static int coremap_getfreepages(unsigned long npages) {
	unsigned int i, pages = 0;
	for (i = last_page_used; i < coremap_size; i++) {
		if (coremap[i].state == FREE) {
			pages += 1;
		} else {
			pages = 0;
		}

		if (pages == npages) {
			int page = i - (npages - 1);
			last_page_used = i;

			return page;
		}
	}

	// check the start of the coremap, but don't count pages wrapping around
	pages = 0;
	for (i = 0; i < last_page_used; i++) {
		if (coremap[i].state == FREE) {
			pages += 1;
		} else {
			pages = 0;
		}

		if (pages == npages) {
			int page = i - (npages - 1);
			last_page_used = i;

			return page;
		}
	}

	return -1;
}

paddr_t coremap_getpages(unsigned long npages) {
	if (npages == 0) {
		panic("Attempting to get 0 pages from the coremap.\n");
	}

	paddr_t paddr;

	if (coremap_initialized) {
		lock_acquire(coremap_lock);

		// find npages consecutive pages in physical memory
		int page = coremap_getfreepages(npages);

		if (page != -1) {
			// we found space
			coremap[page].addr = (vaddr_t) NULL;
			coremap[page].addrspace = (struct addrspace *) NULL;
			coremap[page].page_count = npages;
			coremap[page].state = ALLOCATED;

			unsigned int i;
			for (i = 1; i < npages; i++) {
				coremap[page + i].addr = (vaddr_t) NULL;
				coremap[page + i].addrspace = (struct addrspace *) NULL;
				coremap[page + i].page_count = 0;
				coremap[page + i].state = ALLOCATED;
			}

			coremap_pages_in_use += npages;

			DEBUG(DB_COREMAP, "%u of %u pages in use after allocation of %lu pages starting with page %u.\n",
					coremap_pages_in_use, coremap_size, npages, page);

			paddr = (unsigned long) page * PAGE_SIZE;
		} else {
			// we didn't find space so we need to do some swapping

			// we have no way of handling swapping blocks at this point
			if (npages != 1) {
				panic("Attempting to swap out multiple pages for an allocation, which we can't do!\n");
			}

			// look for a page randomly which is:
			//   not fixed (obviously)
			//   allocated alone (because we have no way of swapping whole allocation blocks)
			//   in an address space (because the page table will store that it was swapped in the first place)
			//   has a virtual address (just a sanity check with the last one)
			int page;

			do {
				page = random() % coremap_size;
			} while (coremap[page].state == FIXED || coremap[page].page_count > 1 ||
					coremap[page].addrspace == NULL || coremap[page].addr == NULL);

			int index = swapfile_prepareswap();

			DEBUG(DB_COREMAP, "Evicting page %d from the coremap and swapping to swapfile index %d.", page, index);

			// tell the page table that we've swapped the page out
			pt_notify_of_swap(coremap[page].addrspace->as_pt, coremap[page].addr, index);

			swapfile_performswap(index, PADDR_TO_KVADDR(1));

			paddr = (unsigned long) page * PAGE_SIZE;
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
				if (coremap[page + i].state == ALLOCATED || coremap[page + i].state == FIXED) {
					coremap[page + i].addr = (vaddr_t) NULL;
					coremap[page + i].addrspace = (struct addrspace *) NULL;
					coremap[page + i].page_count = 0;
					coremap[page + i].state = FREE;
				} else {
					DEBUG(DB_COREMAP, "Attempting free on an unallocated page.\n");
				}
			}

			coremap_pages_in_use -= npages;

			DEBUG(DB_COREMAP, "%u of %u pages in use after free of %lu pages.\n", coremap_pages_in_use, coremap_size, npages);
		} else {
			DEBUG(DB_COREMAP, "Attempting free in the middle of an allocation block.\n");
		}

		lock_release(coremap_lock);
	} else {
		panic("Trying to free 0x%x before the coremap is initialized.\n", paddr);
	}
}

int coremap_ispagefixed(paddr_t paddr) {
	if (paddr % PAGE_SIZE != 0) {
		DEBUG(DB_COREMAP, "Warning: paddr for coremap_ispagefixed is not page-aligned.\n");
	}

	unsigned long page = paddr / PAGE_SIZE;

	return coremap[page].state == FIXED;
}

void coremap_setpagefixed(paddr_t paddr, int fixed) {
	if (paddr % PAGE_SIZE != 0) {
		DEBUG(DB_COREMAP, "Warning: paddr for coremap_setpagefixed is not page-aligned.\n");
	}

	unsigned long page = paddr / PAGE_SIZE;

	if (coremap[page].state != FREE) {
		if (fixed) {
			coremap[page].state = FIXED;
		} else {
			coremap[page].state = ALLOCATED;
		}
	} else {
		DEBUG(DB_COREMAP, "Warning: Attempting to set free page to be %s.\n", (fixed == 0 ? "swappable" : "fixed"));
	}
}

void coremap_getpagevaddr(paddr_t paddr, struct addrspace **addrspace, vaddr_t *vaddr) {
	if (paddr % PAGE_SIZE != 0) {
		DEBUG(DB_COREMAP, "Warning: paddr for coremap_getpagevaddr is not page-aligned.\n");
	}

	unsigned long page = paddr / PAGE_SIZE;

	if (coremap[page].state != FREE) {
		*addrspace = coremap[page].addrspace;
		*vaddr = coremap[page].addr;
	} else {
		DEBUG(DB_COREMAP, "Warning: Attempting to get the vaddr of a free page.\n");
	}
}

void coremap_setpagevaddr(paddr_t paddr, struct addrspace *addrspace, vaddr_t vaddr) {
	if (paddr % PAGE_SIZE != 0) {
		DEBUG(DB_COREMAP, "Warning: paddr for coremap_setpagevaddr is not page-aligned.\n");
	}

	unsigned long page = paddr / PAGE_SIZE;

	if (coremap[page].state != FREE) {
		coremap[page].addrspace = addrspace;
		coremap[page].addr = vaddr;
	} else {
		DEBUG(DB_COREMAP, "Warning: Attempting to get the vaddr of a free page.\n");
	}
}
