#include <syscall.h>

#include <synch.h>
#include <fd.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>
#include <lib.h>
#include <kern/unistd.h>
#include <kern/errno.h>

int sys_write(int fd, const void *buf, size_t nbytes, int *err) {

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
		DEBUG(DB_FSYSCALL, "Write attempted to stdin.\n");

		lock_release(file_table_lock);

		*err = EBADF;
		return -1;
	} else if (fd == 1 || fd == 2) {
		if (file_table[fd] == NULL) {
			DEBUG(DB_FSYSCALL, "Opening %s because it isn't open yet.\n", (fd == 1 ? "stdout" : "stderr"));

			*err = _open(fd, "con:", O_WRONLY, 0);

			if (*err != 0) {
				DEBUG(DB_FSYSCALL, "Error opening %s for write.\n", (fd == 1 ? "stdout" : "stderr"));

				lock_release(file_table_lock);

				return -1;
			}
		}
	}

	// make sure that the file is open and writable
	if (file_table[fd] == NULL) {
		DEBUG(DB_FSYSCALL, "Write attempted on closed file %d.\n", fd);

		lock_release(file_table_lock);

		*err = EBADF;
		return -1;
	} else if (file_table[fd]->flags == O_RDONLY) {
		DEBUG(DB_FSYSCALL, "Write attempted on read-only file %d.\n", fd);

		lock_release(file_table_lock);

		*err = EBADF;
		return -1;
	}

	struct uio output;

	// copy the data to system space since we don't know the address space for the uio
	void *data = kmalloc(nbytes);
	memcpy(data, buf, nbytes);

	// set where the data is and how long it is
	output.uio_iovec.iov_kbase = data;
	output.uio_iovec.iov_len = nbytes;

	output.uio_offset = file_table[fd]->position; // where in the file we want to write to
	output.uio_resid = nbytes; // how much data we can transfer
	output.uio_segflg = UIO_SYSSPACE;
	output.uio_rw = UIO_WRITE;
	output.uio_space = NULL; // the data is in kernel space

	// perform the actual write
	*err = VOP_WRITE(file_table[fd]->node, &output);
	int length = output.uio_offset - file_table[fd]->position;

	// free memory
	kfree(data);

	if (*err == 0) {
		DEBUG(DB_FSYSCALL, "Write started at %d and wrote %d of %d bytes successfully.\n",
				output.uio_offset, length, nbytes);

		// update the offset in the file
		file_table[fd]->position += length;

		lock_release(file_table_lock);

		return length;
	} else {
		lock_release(file_table_lock);

		return -1;
	}
}
