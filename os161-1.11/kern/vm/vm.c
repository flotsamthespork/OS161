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

static void tlb_update(vaddr_t faultaddress, paddr_t paddr, int tlb_idx) {
	u_int32_t ehi, elo;
	int writeable;

	writeable = paddr & PAGE_W_MASK;

	paddr &= PAGE_FRAME;

	ehi = faultaddress;
	elo = paddr | TLBLO_VALID;
	if (writeable) {
		elo |= TLBLO_DIRTY;
	}
	DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
	TLB_Write(ehi, elo, tlb_idx);
}

static int tlb_fault(vaddr_t faultaddress, paddr_t paddr) {
	u_int32_t ehi, elo;
	int tlb_idx = get_tlb_replace_idx();

	// Fault occured, but it was just a TLB Miss and not a bad address
	vmstats_inc(VMSTAT_TLB_FAULT);

	TLB_Read(&ehi, &elo, tlb_idx);

	if (elo & TLBLO_VALID) {
		// Fault occurred and a TLB entry that is currently in use is
		// being replaced with a new one.
		vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);
	} else {
		// Fault occurred and we replaced an invalid TLB entry.
		vmstats_inc(VMSTAT_TLB_FAULT_FREE);
	}

	tlb_update(faultaddress, paddr, tlb_idx);
	return tlb_idx;
}

int vm_fault(int faulttype, vaddr_t faultaddress) {
	// TODO
	paddr_t paddr;
	struct addrspace *as;
	int spl;
	int i, nregions;
	struct region *region;
	struct pagetable *pt;
	vaddr_t rbot, rtop;
	vaddr_t vaddr = faultaddress;

	off_t offset;
	size_t filesize;
	size_t memsize;

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

	pt = as->as_pt;

	paddr = pt_get_paddr(pt, faultaddress, 0, 0);

	if (paddr & PAGE_FREE) {
		nregions = as->as_region_count;

		for (i = 0; i < nregions; i++) {
			region = &as->as_regions[i];
			rbot = region->vaddr;
			rtop = rbot + region->memsize;
			if (vaddr < rtop && vaddr >= rbot) {
				paddr = pt_get_paddr(pt, faultaddress, 1, PAGE_R_MASK | PAGE_W_MASK);

				// Get the address into the tlb so we can write stuff from the elf
				// file without causing another vm_fault
				tlb_fault(faultaddress, paddr);

				offset = region->offset + (off_t) (faultaddress - (region->vaddr & PAGE_FRAME));

				rbot = (faultaddress > region->vaddr ? faultaddress : region->vaddr);
				rtop = (faultaddress + PAGE_SIZE);

				filesize = rtop - rbot;
				memsize = rtop - rbot;

				if (region->vaddr + region->filesize < rtop) {
					filesize = (region->vaddr + region->filesize);
					if (rbot <= filesize) {
						filesize -= rbot;
					} else {
						filesize = 0;
					}
				}

				if (region->vaddr + region->memsize < rtop) {
					memsize = (region->vaddr + region->memsize);
					if (rbot <= memsize) {
						memsize -= rbot;
					} else {
						memsize = 0;
					}
				}

				load_segment(as->as_v, offset, rbot, memsize, filesize, region->permissions & PAGE_X_MASK);

				pt_set_permissions(pt, faultaddress, 1, region->permissions);
				i = TLB_Probe(faultaddress, 0);
				if (i >= 0) {
					tlb_update(faultaddress, (paddr & ~(PAGE_W_MASK | PAGE_R_MASK | PAGE_X_MASK)) | region->permissions, i);
				} else {
					tlb_fault(faultaddress, paddr);
				}

				vmstats_inc(VMSTAT_ELF_FILE_READ);
				vmstats_inc(VMSTAT_PAGE_FAULT_DISK);

				splx(spl);
				return 0;
			}
		}
	}

	// If the page is free or in the swapfile we want to
	// create it or load it from the swapfile
	if (paddr & PAGE_FREE || paddr & PAGE_IN_SWP) {
		paddr = pt_get_paddr(pt, faultaddress, 1, PAGE_R_MASK | PAGE_W_MASK);
	} else {
		vmstats_inc(VMSTAT_TLB_RELOAD);
	}

	switch (faulttype) {
	    case VM_FAULT_READONLY:
	    if (!(paddr & PAGE_W_MASK)) {
	    	splx(spl);
	    	return EFAULT;
	    }
	    // fallthrough to make sure it is readable only
	    case VM_FAULT_READ:
	    if (!(paddr & PAGE_R_MASK)) {
	    	splx(spl);
	    	return EFAULT;
	    }
	    break;
	    case VM_FAULT_WRITE:
	    if (!(paddr & PAGE_W_MASK)) {
	    	splx(spl);
	    	return EFAULT;
	    }
		break;
	}

	tlb_fault(faultaddress, paddr);
	
	splx(spl);
	return 0;
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
