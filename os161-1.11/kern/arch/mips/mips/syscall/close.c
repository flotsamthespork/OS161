#include <syscall.h>

#include <curthread.h>
#include <fd.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <process.h>
#include <synch.h>
#include <vfs.h>

int sys_close(int fd) {

	int retval = 0;

	DEBUG(DB_FSYSCALL, "Closing file handle %d in process %d\n", fd, curthread->t_pid);

	struct lock *file_table_lock = runningprocesses[curthread->t_pid]->p_file_table_lock;
	struct fd **file_table = runningprocesses[curthread->t_pid]->p_file_table;

	lock_acquire(file_table_lock);

	if (fd >= 0 && fd <= 2) {
		// we can't close stdio
		DEBUG(DB_FSYSCALL, "Close attempted on %s.\n", (fd == 0 ? "stdin" : (fd == 1 ? "stdout" : "stderr")));

		retval = EBADF;
	} else if (fd < 0 || fd >= MAX_FILE_HANDLES) {
		// this isn't even a file handle
		DEBUG(DB_FSYSCALL, "Close attempted on invalid handle %d.\n", fd);

		retval = EBADF;
	} else if (file_table[fd] != NULL) {
		_close(file_table, fd);
	} else {
		DEBUG(DB_FSYSCALL, "Invalid file handle %d passed to close\n", fd);

		retval = EBADF;
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
