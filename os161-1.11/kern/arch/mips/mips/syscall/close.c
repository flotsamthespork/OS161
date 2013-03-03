#include <syscall.h>

#include <lib.h>
#include <fd.h>
#include <vfs.h>
#include <kern/unistd.h>
#include <kern/errno.h>

int sys_close(int fd, int *err) {

	int retval;

	// TODO move this to process initialization
	initialize_file_table_if_necessary();

	lock_acquire(file_table_lock);

	if (file_table[fd] != NULL) {
		DEBUG(DB_FSYSCALL, "Closing file handle %d\n", fd);

		// close file
		vfs_close(file_table[fd]->node);

		// free file_table entry
		kfree(file_table[fd]);
		file_table[fd] = NULL;

		DEBUG(DB_FSYSCALL, "Setting retval on successful close\n");

		*err = 0;
		retval = 0;
	} else {
		// This handles stdin, stdout, and stderr since those file_table entries will always be NULL
		DEBUG(DB_FSYSCALL, "Invalid file handle %d passed to close\n", fd);

		*err = EBADF;
		retval = -1;
	}

	lock_release(file_table_lock);

	return retval;

}
