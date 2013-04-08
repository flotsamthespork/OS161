
#include <vm.h>
#include <pt.h>
#include <lib.h>
#include <coremap.h>
#include <addrspace.h>
#include <swapfile.h>
#include <kern/errno.h>
#include <thread.h>
#include <curthread.h>

#include <uw-vmstats.h>



static struct pagetable *_pt_create(int permissions, vaddr_t vaddr) {
	int i;
	struct pagetable *pt = (struct pagetable*)alloc_kpages(1);
	// TODO - FUCKING TIT SACKS
	if (pt==NULL) {
		return NULL;
	}
	vaddr &= PAGE_FRAME;
	vaddr |= SWP_TABLE;
	coremap_setpagevaddr((paddr_t)(pt)-0x80000000, curthread->t_vmspace, vaddr);

	for (i = 0; i < PAGE_SIZE/4; i += 1) {
		((int*)pt)[i] = PAGE_FREE_MASK | permissions;
	}

	return pt;
}


static paddr_t _pt_create_page(vaddr_t vaddr) {
	int i;
	paddr_t paddr = (paddr_t)coremap_getpages(1);
	// TODO - HJOLY FUCK ALSHLFKSAFHLKhj
	if (paddr==(paddr_t)NULL) {
		return (paddr_t)NULL;
	}
	vaddr &= PAGE_FRAME;
	vaddr |= SWP_PAGE;
	coremap_setpagevaddr(paddr, curthread->t_vmspace, vaddr);

	// for (i = 0; i < PAGE_SIZE/4; i += 1) {
	// 	((int*)paddr)[i] = 0;
	// }

	return paddr;
}



static int get_offset(vaddr_t vaddr, int mask, int shift) {
	return (vaddr & (vaddr_t)mask) >> shift;
}



static int get_value(struct pagetable *pt, int offset) {
	return (int) ((int*)pt)[offset];
}



static void set_value(struct pagetable *pt, int offset, int value) {
	((int*)pt)[offset] = value;
}


static int get_page_state_by_value(int value) {
	if (value & PAGE_FREE_MASK) {
		return PAGE_FREE;
	} else if (value & PAGE_IN_MEM_MASK) {
		return PAGE_IN_MEM;
	} else {
		return PAGE_IN_SWP;
	}
}


static int get_page_state(struct pagetable *pt, int offset) {
	return get_page_state_by_value(get_value(pt, offset));
}


// TODO - if *dst = NULL then return ENOMEM from whatever is calling this
static int get_pagetable(struct pagetable *pt,
		vaddr_t vaddr, int create, int permissions,
		struct pagetable **dst) {
	int value, state, offset;

	*dst = NULL;

	offset = get_offset(vaddr, ADDR_UP_MASK, ADDR_UP_SHIFT);
	value = get_value(pt, offset);
	state = get_page_state_by_value(value);

	if (state == PAGE_FREE) {
		if (create) {
			*dst = _pt_create(permissions, vaddr);
			set_value(pt, offset, ((int)*dst) | PAGE_IN_MEM_MASK);
			return 0;
		}
	} else if (state == PAGE_IN_SWP) {
		if (create) {
			// TODO load from swap file
			*dst = _pt_create(0, vaddr);
			swapfile_getpage((value & PAGE_FRAME) >> ADDR_LOW_SHIFT, (void*)dst);
			*dst = (struct pagetable*)((paddr_t)*dst | (paddr_t)(PAGE_IN_MEM | (value & (PAGE_R_MASK | PAGE_W_MASK | PAGE_X_MASK))));
			set_value(pt, offset, ((int)*dst));
			return 0;
		}
	} else {
		*dst = (struct pagetable*) (value & PAGE_FRAME);
		return 0;
	}
	return state;
}


static int get_page(struct pagetable *pt,
		int offset, int create, vaddr_t vaddr, paddr_t *dst) {
	int value, state, permissions;

	value = get_value(pt, offset);
	state = get_page_state_by_value(value);

	permissions = value & (PAGE_R_MASK | PAGE_W_MASK | PAGE_X_MASK);

	if (state == PAGE_FREE) {
		*dst = PAGE_FREE;
		if (create) {
			vmstats_inc(VMSTAT_PAGE_FAULT_ZERO);
			*dst = _pt_create_page(vaddr) | (paddr_t)(PAGE_IN_MEM_MASK | permissions);
			// TODO - set coremap vaddr
			set_value(pt, offset, ((int)*dst));
			return 0;
		}
	} else if (state == PAGE_IN_SWP) {
		*dst = PAGE_IN_SWP;
		if (create) {
			vmstats_inc(VMSTAT_PAGE_FAULT_DISK);
			// TODO - load from swap file
			*dst = _pt_create_page(vaddr);
			swapfile_getpage((value & PAGE_FRAME) >> ADDR_LOW_SHIFT, (void*)dst);
			// TODO - set coremap vaddr
			*dst = *dst | (paddr_t)(PAGE_IN_MEM_MASK | permissions);
			set_value(pt, offset, ((int)*dst));
			return 0;
		}
	} else {
		*dst = (paddr_t) (value & 0xFFFFFFFC);
		return 0;
	}
	return state;
}



static void destroy_nested_pagetable(int value) {
	int i;
	struct pagetable *pt;
	int state = get_page_state_by_value(value);
	if (state == PAGE_FREE) {
		return;
	}

	if (state == PAGE_IN_SWP) {
		// TODO - swap it in and set pt
	} else {
		pt = (struct pagetable*) (value & PAGE_FRAME);
	}

	// Go through and free all the pages allocated
	for (i = 0; i < PAGE_SIZE/4; i += 1) {
		value = get_value(pt, i);
		state = get_page_state_by_value(value);

		if (state == PAGE_FREE) {
			continue;
		}

		if (state == PAGE_IN_SWP) {
			// TODO - remove the page from swap file
		} else {
			coremap_freepages((paddr_t) (value & PAGE_FRAME));
		}
	}

	free_kpages((vaddr_t)pt);
}



struct pagetable *pt_create() {
	struct pagetable *pt = _pt_create(0, (vaddr_t)NULL);
	coremap_setpagefixed((paddr_t)(pt)-0x80000000, 1);
	return pt;
}


void pt_destroy(struct pagetable *pt) {
	int i;
	for (i = 0; i < PAGE_SIZE/4; i += 1) {
		destroy_nested_pagetable(get_value(pt, i));
	}
	free_kpages((vaddr_t)pt);
}

paddr_t pt_get_paddr(struct pagetable *pt, vaddr_t vaddr,
		int create, int permissions) {
	int offset;
	struct pagetable *spt;
	paddr_t paddr;

	get_pagetable(pt, vaddr, create, permissions, &spt);
	if (spt == NULL) {
		return (paddr_t) PAGE_FREE;
	} else {
		offset = get_offset(vaddr, ADDR_LOW_MASK, ADDR_LOW_SHIFT);
		get_page(spt, offset, create, vaddr, &paddr);
		return paddr;
	}
}

int pt_set_permissions(struct pagetable *pt, vaddr_t vaddr,
		int create, int permissions) {
	int offset, value;
	paddr_t paddr;
	struct pagetable *spt;

	get_pagetable(pt, vaddr, create, permissions, &spt);
	if (spt != NULL) {
		offset = get_offset(vaddr, ADDR_LOW_MASK, ADDR_LOW_SHIFT);
		value = get_page(spt, offset, create, vaddr, &paddr);
		if (!value) {
			value = get_value(spt, offset) & ~(PAGE_R_MASK | PAGE_X_MASK | PAGE_W_MASK);
			set_value(spt, offset, value | permissions);
			return 1;
		}
	}
	return 0;
}

static int vaddr_overlap(vaddr_t l1, vaddr_t r1, vaddr_t l2, vaddr_t r2) {
	//  |(l1)    |(l2)  |(r2)   |(r1)
	if (r1 >= r2 && r2 >= l1) return 1;
	//  |(l1)    |(l2)  |(r1)   |(r2)
	if (l1 <= l2 && r1 > l2) return 1;
	
	return 0;
}

int pt_copy(struct pagetable *dst, struct addrspace *as) {
	int idx_count = PAGE_SIZE/4;
	int pt_idx1, pt_idx2, r_idx;
	int value, in_region;
	int state, permissions;
	struct pagetable *pt1 = as->as_pt;
	struct pagetable *pt2;
	struct region *region;
	vaddr_t vaddr, rbot, rtop;
	paddr_t paddr1, paddr2;

	for (pt_idx1 = 0; pt_idx1 < idx_count; pt_idx1++) {
		value = get_value(pt1, pt_idx1);
		state = get_page_state_by_value(value);
		if (state != PAGE_FREE) {
			vaddr = (vaddr_t)(pt_idx1 << (ADDR_UP_SHIFT));
			permissions = value & (PAGE_W_MASK | PAGE_R_MASK | PAGE_X_MASK);

			// If the page is in the swapfile we need it to be
			// back in memory so we call get_pagetable with create
			get_pagetable(pt1, vaddr, 1, permissions, &pt2);

			for (pt_idx2 = 0; pt_idx2 < idx_count; pt_idx2++) {
				value = get_value(pt2, pt_idx2);
				state = get_page_state_by_value(value);

				if (state != PAGE_FREE) {
					vaddr = (vaddr_t)( (pt_idx1 << (ADDR_UP_SHIFT)) + (pt_idx2 << (ADDR_LOW_SHIFT)) );
					permissions = value & (PAGE_W_MASK | PAGE_R_MASK | PAGE_X_MASK);
					in_region = 0;

					for (r_idx = 0; r_idx < as->as_region_count &&
							!(permissions & PAGE_W_MASK); r_idx++) {
						region = &as->as_regions[r_idx];
						rbot = region->vaddr;
						rtop = rbot + region->memsize;

						if (vaddr_overlap(vaddr, vaddr+PAGE_SIZE,
										rbot, rtop)) {
							in_region = 1;
							break;
						}
					}

					// We don't want to copy memory that is contained
					// within a defined region because it will be loaded
					// properly on a vm_fault with permissions and
					// everything set correctly.
					if (!in_region) {
						// TODO - force page to load with get_page
						// TODO - copy the page

						// Get the page and its physical address
						get_page(pt2, pt_idx2, 1, vaddr, &paddr1);

						coremap_setpagefixed(paddr1, 1);

						paddr2 = pt_get_paddr(dst, vaddr, 1, permissions);

						coremap_setpagefixed(paddr2, 1);

						memmove((void *)(PADDR_TO_KVADDR(paddr2) & PAGE_FRAME),
								(const void *)(PADDR_TO_KVADDR(paddr1) & PAGE_FRAME), PAGE_SIZE);

						coremap_setpagefixed(paddr1, 0);
						coremap_setpagefixed(paddr2, 0);

					}
				}
			}
		}
	}

	return 0;
}

void pt_notify_of_swap(struct pagetable *pt, vaddr_t vaddr, int index) {
	int offset, value;
	paddr_t paddr;
	struct pagetable *spt;

	if (vaddr & SWP_PAGE) {
		get_pagetable(pt, vaddr, 1, 0, &spt);
		if (spt != NULL) {
			offset = get_offset(vaddr, ADDR_LOW_MASK, ADDR_LOW_SHIFT);
			value = get_page(spt, offset, 1, vaddr, &paddr);
			if (!value) {
				value = (get_value(spt, offset) & PAGE_FRAME) & ~(PAGE_FREE | PAGE_IN_MEM);
				value |= PAGE_IN_SWP;
				set_value(spt, offset, value | (index << ADDR_LOW_SHIFT));
				return;
			}
		}
	} else if (vaddr & SWP_TABLE) {
		get_pagetable(pt, vaddr, 1, 0, &spt);
		if (spt != NULL) {
			offset = get_offset(vaddr, ADDR_UP_MASK, ADDR_UP_SHIFT);
			value = (get_value(pt, offset) & PAGE_FRAME) & ~(PAGE_FREE | PAGE_IN_MEM);
			value |= PAGE_IN_SWP;
			set_value(pt, offset, value | (index << ADDR_LOW_SHIFT));
			return;
		}
	} else {
		panic("STOP BEING A RETARD AND SET THE FLAGS PROPER.");
	}
}
