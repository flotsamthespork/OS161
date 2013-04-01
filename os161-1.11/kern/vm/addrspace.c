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

	// TODO start
	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
	// TODO end

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

	// TODO start
	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	assert(new->as_pbase1 != 0);
	assert(new->as_pbase2 != 0);
	assert(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);

	// TODO end
	
	*ret = new;
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	assert(as != NULL);
	assert(as->as_pbase1 != (paddr_t) NULL);
	assert(as->as_pbase2 != (paddr_t) NULL);
	assert(as->as_stackpbase != (paddr_t) NULL);

	coremap_freepages(as->as_pbase1);
	coremap_freepages(as->as_pbase2);
	coremap_freepages(as->as_stackpbase);
	
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
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{

	// TODO
	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
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
	// TODO

	assert(as->as_pbase1 == 0);
	assert(as->as_pbase2 == 0);
	assert(as->as_stackpbase == 0);

	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

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

	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	struct addrspace *as;
	int spl;

	spl = splhigh();

	ptr &= PAGE_FRAME;

	as = curthread->t_vmspace;
	if (as == NULL) {
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	assert(as->as_vbase1 != 0);
	assert(as->as_pbase1 != 0);
	assert(as->as_npages1 != 0);
	assert(as->as_vbase2 != 0);
	assert(as->as_pbase2 != 0);
	assert(as->as_npages2 != 0);
	assert(as->as_stackpbase != 0);
	assert((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	assert((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	assert((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	assert((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	assert((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;

	if (ptr >= vbase1 && ptr < vtop1) {}
	else if (ptr >= vbase2 && ptr < vtop2) {}
	else if (ptr >= stackbase && ptr < stacktop) {}
	else {
		splx(spl);
		return EFAULT;
	}
	splx(spl);
	return 0;
}
