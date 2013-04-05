#include <vm.h>

#include <addrspace.h>
#include <coremap.h>
#include <curthread.h>
#include <kern/errno.h>
#include <lib.h>
#include <machine/spl.h>
#include <machine/tlb.h>
#include <swapfile.h>
#include <thread.h>
#include <types.h>
#include <uw-vmstats.h>
#include <pt.h>

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
	swapfile_bootstrap();

	vmstats_init();
}

#if OPT_A3
void vm_shutdown() {
	coremap_shutdown();
	// swapfile_shutdown();

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
	int writeable;

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
		return EFAULT;
	}

	paddr = pt_get_paddr(as->as_pt, faultaddress, 1, PAGE_W_MASK | PAGE_R_MASK);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
	    if (paddr & PAGE_W_MASK) {
	    	splx(spl);
	    	return EFAULT;
	    }
	    // fallthrough to make sure it is read only
	    case VM_FAULT_READ:
	    if (!(paddr & PAGE_R_MASK)) {
	    	splx(spl);
	    	return EFAULT;
	    }
	    break;
	    case VM_FAULT_WRITE:
	    // if (!(paddr & PAGE_W_MASK)) {
	    // 	splx(spl);
	    // 	return EFAULT;
	    // }
		break;
	}

	writeable = paddr & PAGE_W_MASK;


	paddr &= PAGE_FRAME;

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
	elo = paddr | TLBLO_VALID;
	// if (writeable) {
		elo |= TLBLO_DIRTY;
	// }
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
