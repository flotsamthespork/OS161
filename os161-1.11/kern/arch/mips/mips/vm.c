#include <vm.h>

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

#define DUMBVM_STACKPAGES    12

static int next_tlb_idx = 0;

static int get_tlb_replace_idx() {
	int idx = next_tlb_idx;
	next_tlb_idx = (next_tlb_idx + 1) % NUM_TLB;
	return idx;
}

void vm_bootstrap() {
	coremap_bootstrap();

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
	paddr_t paddr;
	int tlb_idx;
	u_int32_t ehi, elo;


	struct addrspace *as;
	int spl;
	int i;
	int permissions;
	vaddr_t vbase, vtop;

	spl = splhigh();

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
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
		 splx(spl);
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	assert(as->as_stackpbase != 0);
	assert((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	for (i = 0; i < as->as_nregions; i++) {

		assert(as->as_vbase[i] != 0);
		assert(as->as_pbase[i] != 0);
		assert(as->as_npages[i] != 0);
		assert((as->as_vbase[i] & PAGE_FRAME) == as->as_vbase[i]);
		assert((as->as_pbase[i] & PAGE_FRAME) == as->as_pbase[i]);

		vbase = as->as_vbase[i];
		vtop = vbase + as->as_npages[i] * PAGE_SIZE;
		if (faultaddress >= vbase && faultaddress < vtop) {
			paddr = (faultaddress - vbase) + as->as_pbase[i];
			break;
		}
	}

	if (i < as->as_nregions) {
		// TODO - check stack 
	}

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

vaddr_t alloc_kpages(int npages) {
	int spl = splhigh();

	paddr_t paddr = coremap_getpages(npages);

	splx(spl);

	if (paddr == (paddr_t) NULL) {
		return (paddr_t) NULL;
	} else {
		return PADDR_TO_KVADDR(paddr);
	}
}

void free_kpages(vaddr_t addr) {
	int spl = splhigh();

	coremap_freepages(KVADDR_TO_PADDR(addr));

	splx(spl);
}
