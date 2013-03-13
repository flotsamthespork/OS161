#include <syscall.h>

#include <addrspace.h>
#include <fd.h>
#include <kern/errno.h>
#include <lib.h>
#include <fd.h>
#include <vfs.h>

int sys_open(const char *filename, int flags, int mode, int *err) {

	// make sure that we've been given a valid pointer for the file name
	// TODO talk to patrick about as_valid_ptr since it returns weirdly (0 on a valid ptr?)
	if (as_valid_ptr(filename)) {
		*err = EFAULT;
		return -1;
	}

	int retval = -1;

	// TODO move this to process initialization
	initialize_file_table_if_necessary();

	lock_acquire(file_table_lock);

	int i;
	for (i = FIRST_FILE_HANDLE; i < MAX_FILE_HANDLES; i++) {
		if (file_table[i] == NULL) {
			// TODO do we need to free the duplicated filename?
			*err = _open(i, filename, flags, mode);

			retval = i;
			break;
		}
	}

	lock_release(file_table_lock);

	return retval;

}

/** Performs the actual opening given a file handle so that we can use this
 * to open stdio automatically from read/write. **/
int _open(int fd, const char *filename, int flags, int mode) {

	((void) mode); // hide unused argument warning

	// we can assert here since the kernel is the only thing calling us directly
	assert(fd >= 0 && fd < MAX_FILE_HANDLES);
	assert(file_table[fd] == NULL);

	file_table[fd] = (struct fd *) kmalloc(sizeof(struct fd));

	file_table[fd]->name = kstrdup(filename);
	file_table[fd]->flags = 0x3 & flags; // strip out flags that aren't O_WRONLY, O_RDONLY, O_RDWR
	file_table[fd]->position = 0;

	// duplicate the filename for opening because vfs_open doesn't preserve it
	char * kfilename = kstrdup(filename);
	int err = vfs_open(kfilename, flags, &(file_table[fd]->node));
	kfree(kfilename);

	DEBUG(DB_FSYSCALL, "vfs_open returned %d when opening %s as %d in %d\n", err, file_table[fd]->name, file_table[fd]->flags, fd);

	return err;
}
