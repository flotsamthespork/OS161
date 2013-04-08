
#ifndef __PT_H__
#define __PT_H__

#define ADDR_UP_MASK 			(0xFFC00000)
#define ADDR_UP_SHIFT			(22)
#define ADDR_LOW_MASK 			(0x003FF000)
#define ADDR_LOW_SHIFT 			(12)

#define PAGE_FREE				(0x1)
#define PAGE_IN_MEM 			(0x2)
#define PAGE_IN_SWP				(0x4)

#define PAGE_FREE_MASK 			(0x00000001)
#define PAGE_IN_MEM_MASK 		(0x00000002)
#define PAGE_W_MASK 			(0x00000004)
#define PAGE_R_MASK 			(0x00000008)
#define PAGE_X_MASK 			(0x00000010)

#include <addrspace.h>

struct pagetable {};

struct pagetable *pt_create();
void pt_destroy(struct pagetable *pt);
paddr_t pt_get_paddr(struct pagetable *pt, vaddr_t vaddr, int create, int permissions);
int pt_set_permissions(struct pagetable *pt, vaddr_t vaddr, int create, int permissions);
int pt_copy(struct pagetable *dst, struct addrspace *as);

void pt_notify_of_swap(struct pagetable *pt, vaddr_t vaddr, int index);

#endif
