#include <swapfile.h>

#include <kern/unistd.h>
#include <lib.h>
#include <synch.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>

static int swapfile_entries[SWAPFILE_MAX_PAGES];

static struct vnode *swapfile;

static struct lock *swapfile_lock;

// the number of used entries in the swapfile (for debugging purposes)
static unsigned int swapfile_pages_in_use = 0;

void swapfile_bootstrap() {
	int i;
	for (i = 0; i < SWAPFILE_MAX_PAGES; i++) {
		swapfile_entries[i] = 0;
	}

	DEBUG(DB_SWAPFILE, "Opening swapfile %s of size %d (%d pages).", SWAPFILE_NAME, SWAPFILE_MAX_SIZE, SWAPFILE_MAX_PAGES);

	char * swapfile_name = kstrdup(SWAPFILE_NAME); // copy the name because vfs_open does funny things with it
	int err = vfs_open(swapfile_name, O_RDWR | O_CREAT | O_TRUNC, &swapfile);
	kfree(swapfile_name);
	if (err != 0) panic("Error %d opening swapfile %s\n.", err, SWAPFILE_NAME);

	swapfile_lock = lock_create("swapfile_lock");
	if (swapfile_lock == NULL) panic("Unable to instantiate swapfile_lock.\n");
}

void swapfile_shutdown() {
	vfs_close(swapfile);

	lock_destroy(swapfile_lock);
}

int swapfile_storepage(void *source) {
	lock_acquire(swapfile_lock);

	int i;
	for (i = 0; i < SWAPFILE_MAX_PAGES; i++) {
		if (!swapfile_entries[i]) break;
	}

	if (i == SWAPFILE_MAX_PAGES) panic("Out of swap space");

	DEBUG(DB_SWAPFILE, "Storing page at %x to swapfile page %d. %d of %d pages are in use.\n",
			(unsigned int) source, i, swapfile_pages_in_use, SWAPFILE_MAX_PAGES);

	// construct a uio to handle the write
	struct uio operation;
	operation.uio_iovec.iov_kbase = source;
	operation.uio_iovec.iov_len = PAGE_SIZE;

	operation.uio_offset = i * PAGE_SIZE;
	operation.uio_resid = PAGE_SIZE;
	operation.uio_segflg = UIO_SYSSPACE;
	operation.uio_rw = UIO_WRITE;
	operation.uio_space = NULL;

	int err = VOP_WRITE(swapfile, &operation);
	int length = operation.uio_offset - i * PAGE_SIZE;

	assert(err == 0);
	assert(length == PAGE_SIZE);

	swapfile_entries[i] = 1;

	lock_release(swapfile_lock);

	return i;
}

int swapfile_getpage(int i, void *dest) {
	lock_acquire(swapfile_lock);

	assert(i >= 0);
	assert(i < SWAPFILE_MAX_PAGES);
	assert(swapfile_entries[i] == 1);

	// construct a uio to handle the write
	struct uio operation;
	operation.uio_iovec.iov_kbase = dest;
	operation.uio_iovec.iov_len = PAGE_SIZE;

	operation.uio_offset = i * PAGE_SIZE;
	operation.uio_resid = PAGE_SIZE;
	operation.uio_segflg = UIO_SYSSPACE;
	operation.uio_rw = UIO_READ;
	operation.uio_space = NULL;

	int err = VOP_READ(swapfile, &operation);
	int length = operation.uio_offset - i * PAGE_SIZE;

	assert(err == 0);
	assert(length == PAGE_SIZE);

	swapfile_entries[i] = 0;

	lock_release(swapfile_lock);

	return i;
}
