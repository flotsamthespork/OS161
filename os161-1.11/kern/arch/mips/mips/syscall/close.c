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

	// TODO refactor this to be ordered a bit nicer
	if (fd >= 0 && fd <= 2) {
		// we can't close stdio
		DEBUG(DB_FSYSCALL, "Close attempted on %s.\n", (fd == 0 ? "stdin" : (fd == 1 ? "stdout" : "stderr")));

		*err = EBADF;
		retval = -1;
	} else if (fd < 0 || fd >= MAX_FILE_HANDLES) {
		// this isn't even a file handle
		DEBUG(DB_FSYSCALL, "Close attempted on invalid handle %d.\n", fd);

		*err = EBADF;
		retval = -1;
	} else if (file_table[fd] != NULL) {
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
