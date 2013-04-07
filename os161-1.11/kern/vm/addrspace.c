#include <addrspace.h>

#include <coremap.h>
#include <curthread.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <thread.h>
#include <types.h>
#include <uw-vmstats.h>
#include <vnode.h>

#include "opt-A3.h"

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

#define DUMBVM_STACKPAGES    12

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->as_region_count = 0;

	as->as_pt = pt_create();
	if (as->as_pt == NULL) {
		kfree(as);
		return NULL;
	}

	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	if (pt_copy(new->as_pt, old)) {
		as_destroy(new);
		return ENOMEM;
	}

	VOP_INCOPEN(old->as_v);
	VOP_INCREF(old->as_v);

	new->as_v = old->as_v;

	// TODO - copy
	new->as_region_count = old->as_region_count;
	memmove((void *)&new->as_regions,
		(const void *)(paddr_t)&old->as_regions,
		(sizeof(struct region) * new->as_region_count));
	
	*ret = new;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	assert(as != NULL);
	assert(as->as_pt != NULL);

	pt_destroy(as->as_pt);
	
	kfree(as);
}

void
as_activate(struct addrspace *as)
{
	// TODO

	int i, spl;

	(void)as;

	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	// TLB has been entirely invalidated
	vmstats_inc(VMSTAT_TLB_INVALIDATE);

	splx(spl);
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, off_t offset,
		 size_t memsize, size_t filesize, vaddr_t vaddr,
		 int readable, int writeable, int executable)
{
	// memsize was what was originally passed in

	struct pagetable *pt = as->as_pt;
	int permissions = 0;
	int error;
	int nregions;
	struct region *region;

	if (readable) { permissions |= PAGE_R_MASK; }
	if (writeable) { permissions |= PAGE_W_MASK; }
	if (executable) { permissions |= PAGE_X_MASK; }


	nregions = as->as_region_count;

	if (nregions == MAX_REGIONS) {
		panic("vm: too many regions defined. OH GOD, EVERYBODY PANIC.");
	}

	region = &as->as_regions[nregions];
	region->offset = offset;
	region->memsize = memsize;
	region->filesize = filesize;
	region->vaddr = vaddr;
	region->permissions = permissions;

	as->as_region_count = nregions + 1;

	return 0;
}


paddr_t as_get_paddr(struct addrspace *as, vaddr_t vaddr) {
	// TODO - traverse regions
	int i, nregions;
	struct region *region;
	struct pagetable *pt = as->as_pt;
	vaddr_t rbot, rtop;
	paddr_t paddr;

	paddr = pt_get_paddr(pt, vaddr, 0, 0);

	if (paddr & PAGE_FREE) {
		nregions = as->as_region_count;

		for (i = 0; i < nregions; i++) {
			region = &as->as_regions[i];
			rbot = region->vaddr;
			rtop = rbot + region->memsize;
			if (vaddr < rtop && vaddr >= rbot) {
				paddr = pt_get_paddr(pt, vaddr, 1, PAGE_R_MASK | PAGE_W_MASK);
				// TODO - load tlb shit so we dont endlessly vm_fault
				// load_region(as->as_v, region, )

				pt_set_permissions(pt, vaddr, 1, region->permissions);
			}
			break;
		}
	}

	// If the page is free or in the swapfile we want to
	// create it or load it from the swapfile
	if (paddr & PAGE_FREE || paddr & PAGE_IN_SWP) {
		paddr = pt_get_paddr(pt, vaddr, 1, PAGE_R_MASK | PAGE_W_MASK);
	}
}

static
paddr_t
getppages(unsigned long npages)
{
	return coremap_getpages(npages);
}

int
as_prepare_load(struct addrspace *as)
{

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	// TODO

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	// TODO

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;
	
	return 0;
}

int as_valid_ptr(vaddr_t ptr) {
	// TODO
	vaddr_t rbot, rtop;
	paddr_t paddr;
	struct addrspace *as;
	struct region *region;
	int spl, i;

	spl = splhigh();

	as = curthread->t_vmspace;
	(void)ptr;
	if (as == NULL) {
		splx(spl);
		return EFAULT;
	}

	// TODO - check regions

	for (i = 0; i < as->as_region_count; i++) {
		region = &as->as_regions[i];
		rbot = region->vaddr;
		rtop = rbot + region->memsize;
		if (ptr < rtop && ptr >= rbot) {
			splx(spl);
			return 0;
		}
	}

	paddr = pt_get_paddr(as->as_pt, ptr & PAGE_FRAME, 0, 0);

	if (paddr & PAGE_FREE) {
		splx(spl);
		return EFAULT;
	}

	splx(spl);
	return 0;
}
