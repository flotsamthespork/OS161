#include <syscall.h>

#include <curthread.h>
#include <fd.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <thread.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>

int sys_read(int fd, void *buf, size_t buflen, int *err) {

	// make sure that we've been given a valid pointer for the file name
	// TODO talk to patrick about as_valid_ptr since it returns weirdly (0 on a valid ptr?)
	if (as_valid_ptr(buf)) {
		*err = EFAULT;
		return -1;
	}

	// TODO move this to process initialization
	initialize_file_table_if_necessary();

	// make sure that we're using a file handle which is actually open
	if (fd < 0 || fd > MAX_FILE_HANDLES - 1) {
		DEBUG(DB_FSYSCALL, "Invalid file handle %d\n", fd);

		*err = EBADF;
		return -1;
	}

	lock_acquire(file_table_lock);

	// handle the special cases (stdin, stdout, and stderr)
	if (fd == 0) {
		if (file_table[fd] == NULL) {
			DEBUG(DB_FSYSCALL, "Opening stdin because it isn't open yet.\n");

			*err = _open(fd, "con:", O_RDONLY, 0);

			if (*err != 0) {
				DEBUG(DB_FSYSCALL, "Error opening stdin for read.\n");

				lock_release(file_table_lock);

				return -1;
			}
		}
	} else if (fd == 1 || fd == 2) {
		DEBUG(DB_FSYSCALL, "Read attempted from %s.\n", (fd == 1 ? "stdout" : "stderr"));

		lock_release(file_table_lock);

		*err = EBADF;
		return -1;
	}

	// make sure that the file is open
	if (file_table[fd] == NULL) {
		DEBUG(DB_FSYSCALL, "Read attempted on closed file %d.\n", fd);

		lock_release(file_table_lock);

		*err = EBADF;
		return -1;
	} else if (file_table[fd]->flags == O_WRONLY) {
		DEBUG(DB_FSYSCALL, "Read attempted on write-only file %d.\n", fd);

		lock_release(file_table_lock);

		*err = EBADF;
		return -1;
	}

	struct uio output;

//	// create a buffer in kernel space which we'll copy back into user space
//	// TODO do we need to copy it or can we just use curthread->aspace
//	void *data = kmalloc(buflen);

	// set where the data is and how long it is
	output.uio_iovec.iov_kbase = buf;
	output.uio_iovec.iov_len = buflen;

	output.uio_offset = file_table[fd]->position; // where in the file we want to read from
	output.uio_resid = buflen; // how much data we can transfer
	output.uio_segflg = UIO_USERSPACE;
	output.uio_rw = UIO_READ;
	output.uio_space = curthread->t_vmspace;

	// perform the actual read
	*err = VOP_READ(file_table[fd]->node, &output);
	int length = output.uio_offset - file_table[fd]->position;

//	// copy the memory back to user space and then free
//	memcpy(buf, data, length);
//	kfree(data);

	if (*err == 0) {
		// update the position in the file
		file_table[fd]->position += length;

		lock_release(file_table_lock);

		return length;
	} else {
		lock_release(file_table_lock);

		return -1;
	}

}
