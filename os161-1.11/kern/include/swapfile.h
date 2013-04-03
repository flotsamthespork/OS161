#ifndef _SWAPFILE_H_
#define _SWAPFILE_H_

#include <vm.h>

#define SWAPFILE_NAME "emu0:/swapfile"
#define SWAPFILE_MAX_SIZE (9 * 1024 * 1024)
#define SWAPFILE_MAX_PAGES (SWAPFILE_MAX_SIZE / PAGE_SIZE)

void swapfile_bootstrap();
void swapfile_shutdown();

/** Stores a page starting at the source address and returns the index
 * of the swapfile entry or -1 if there was no room in the swapfile.
 * The source address must be page-aligned. **/
int swapfile_storepage(void *source);

/** Gets the given page from the swapfile and stores it in the destination
 * address. The entry from the swapfile is then cleared and reusable for
 * later swapping. **/
int swapfile_getpage(int index, void *dest);

#endif
