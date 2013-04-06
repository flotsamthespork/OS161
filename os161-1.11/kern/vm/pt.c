
#include <vm.h>
#include <pt.h>
#include <lib.h>
#include <coremap.h>
#include <kern/errno.h>

#include <uw-vmstats.h>



static struct pagetable *_pt_create(int permissions) {
	int i;
	struct pagetable *pt = (struct pagetable*)alloc_kpages(1);
	if (pt==NULL) {
		return NULL;
	}

	for (i = 0; i < PAGE_SIZE/4; i += 1) {
		((int*)pt)[i] = PAGE_FREE_MASK | permissions;
	}

	return pt;
}


static paddr_t _pt_create_page() {
	int i;
	paddr_t paddr = (paddr_t)coremap_getpages(1);
	if (paddr==(paddr_t)NULL) {
		return (paddr_t)NULL;
	}

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
			*dst = _pt_create(permissions);
			set_value(pt, offset, ((int)*dst) | PAGE_IN_MEM_MASK);
			return 0;
		}
	} else if (state == PAGE_IN_SWP) {
		if (create) {
			// TODO load from swap file
			return 0;
		}
	} else {
		*dst = (struct pagetable*) (value & PAGE_FRAME);
		return 0;
	}
	return state;
}


static int get_page(struct pagetable *pt,
		int offset, int create, paddr_t *dst) {
	int value, state, permissions;

	value = get_value(pt, offset);
	state = get_page_state_by_value(value);

	permissions = value & (PAGE_R_MASK | PAGE_W_MASK | PAGE_X_MASK);

	if (state == PAGE_FREE) {
		*dst = PAGE_FREE;
		if (create) {
			vmstats_inc(VMSTAT_PAGE_FAULT_ZERO);
			*dst = _pt_create_page() | (paddr_t)(PAGE_IN_MEM_MASK | permissions);
			set_value(pt, offset, ((int)*dst));
			return 0;
		}
	} else if (state == PAGE_IN_SWP) {
		*dst = PAGE_IN_SWP;
		if (create) {
			vmstats_inc(VMSTAT_PAGE_FAULT_DISK);
			// TODO - load from swap file
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
	return _pt_create(0);
}


void pt_destroy(struct pagetable *pt) {
	int i;
	for (i = 0; i < PAGE_SIZE/4; i += 1) {
		destroy_nested_pagetable(get_value(pt, i));
	}
	free_kpages((vaddr_t)pt);
}

int pt_define_region(struct pagetable *pt, vaddr_t vaddr,
					size_t size, int permissions) {
	vaddr_t i, region_end;
	struct pagetable *subpt;

	region_end = vaddr + (vaddr_t) (size + PAGE_SIZE - 1);

	for (i = vaddr; i < region_end; i += PAGE_SIZE) {
		get_pagetable(pt, i, 1, permissions, &subpt);
		if (subpt == NULL) {
			return ENOMEM;
		}
	}

	return 0;
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
		get_page(spt, offset, create, &paddr);
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
		value = get_page(spt, offset, create, &paddr);
		if (!value) {
			value = get_value(spt, offset) & ~(PAGE_R_MASK | PAGE_X_MASK | PAGE_W_MASK);
			set_value(spt, offset, value | permissions);
			return 1;
		}
	}
	return 0;
}
