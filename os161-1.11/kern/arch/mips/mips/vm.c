#include <vm.h>

#include <addrspace.h>
#include <curthread.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <thread.h>
#include <types.h>
#include <uw-vmstats.h>

#include "opt-A3.h"

#define DUMBVM_STACKPAGES    12

static int next_tlb_idx = 0;

static int get_tlb_replace_idx() {
	int idx = next_tlb_idx;
	next_tlb_idx = (next_tlb_idx + 1) % NUM_TLB;
	return idx;
}

void vm_bootstrap() {
	// TODO
	vmstats_init();
}

#if OPT_A3
void vm_shutdown() {
	// TODO
	vmstats_print();
}
#endif

int vm_fault(int faulttype, vaddr_t faultaddress) {
	// TODO
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int tlb_idx;
	u_int32_t ehi, elo;
	struct addrspace *as;
	int spl;

	spl = splhigh();

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
		/* We always create pages read-write, so we can't get this */
		panic("dumbvm: got VM_FAULT_READONLY\n");
		break;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		splx(spl);
		return EINVAL;
	}

	as = curthread->t_vmspace;
	if (as == NULL) {
		/*
		 * No address space set up. This is probably a kernel
		 * fault early in boot. Return EFAULT so as to panic
		 * instead of getting into an infinite faulting loop.
		 */
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

	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		splx(spl);
		return EFAULT;
	}

	/* make sure it's page-aligned */
	assert((paddr & PAGE_FRAME)==paddr);

	// for (i=0; i<NUM_TLB; i++) {
	// 	TLB_Read(&ehi, &elo, i);
	// 	if (elo & TLBLO_VALID) {
	// 		continue;
	// 	}
	// 	ehi = faultaddress;
	// 	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	// 	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	// 	TLB_Write(ehi, elo, i);
	// 	splx(spl);
	// 	return 0;
	// }

	// Fault occured, but it was just a TLB Miss and not a bad address
	vmstats_inc(VMSTAT_TLB_FAULT);

	tlb_idx = get_tlb_replace_idx();
	TLB_Read(&ehi, &elo, tlb_idx);

	if (elo & TLBLO_VALID) {
		// Fault occurred and a TLB entry that is currently in use is
		// being replaced with a new one.
		vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);
	} else {
		// Fault occurred and we replaced an invalid TLB entry.
		vmstats_inc(VMSTAT_TLB_FAULT_FREE);
	}

	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	TLB_Write(ehi, elo, tlb_idx);
	splx(spl);
	return 0;


	// kprintf("dumbvm: Ran out of TLB entries - cannot handle page fault\n");
	// splx(spl);
	// return EFAULT;
}

static
paddr_t
getppages(unsigned long npages)
{
	int spl;
	paddr_t addr;

	spl = splhigh();

	addr = ram_stealmem(npages);

	splx(spl);
	return addr;
}

vaddr_t alloc_kpages(int npages) {
	// TODO

	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}

void free_kpages(vaddr_t addr) {
	// TODO

	(void)addr;
}
