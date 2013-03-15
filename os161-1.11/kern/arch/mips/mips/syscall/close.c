#include <syscall.h>

#include <curthread.h>
#include <fd.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <process.h>
#include <vfs.h>

int sys_close(int fd, int *err) {

	int retval;

	// TODO return the error value? why am I putting it in *err?

	DEBUG(DB_FSYSCALL, "Closing file handle %d in process %d\n", fd, curthread->t_pid);

	struct lock *file_table_lock = runningprocesses[curthread->t_pid]->p_file_table_lock;
	struct fd **file_table = runningprocesses[curthread->t_pid]->p_file_table;

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
		_close(file_table, fd);

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

/** Performs the actual closing given a file handle so that this can
 * be used directly by other kernel code.
 *
 * Note: This function assumes that the arguments are valid.**/
void _close(struct fd *file_table[], int fd) {
	DEBUG(DB_FSYSCALL, "Closing file handle %d\n", fd);

	// free file name
	kfree(file_table[fd]->name);

	// close file
	vfs_close(file_table[fd]->node);

	// free file_table entry
	kfree(file_table[fd]);
	file_table[fd] = NULL;
}
