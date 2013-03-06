#include <syscall.h>

#include <fd.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>
#include <lib.h>
#include <kern/unistd.h>
#include <kern/errno.h>

int sys_write(int fd, const void *buf, size_t nbytes, int *err) {

	// make sure that we're using a file handle which is actually open
	if (fd < 0 || fd > MAX_FILE_HANDLES - 1) {
		DEBUG(DB_FSYSCALL, "Invalid file handle %d\n", fd);

		*err = EBADF;
		return -1;
	}

	// handle the special cases (stdin, stdout, and stderr)
	if (fd == 0) {
		DEBUG(DB_FSYSCALL, "Write attempted to stdin.\n");

		*err = EBADF;
		return -1;
	} else if (fd == 1 || fd == 2) {
		if (file_table[fd] == NULL) {
			DEBUG(DB_FSYSCALL, "Opening %s because it isn't open yet.\n", (fd == 1 ? "stdout" : "stderr"));

			*err = _open(fd, "con:", O_WRONLY, 0);

			if (*err != 0) {
				DEBUG(DB_FSYSCALL, "Error opening %s for write.\n", (fd == 1 ? "stdout" : "stderr"));

				return -1;
			}
		}
	}

	// make sure that the file is open
	if (file_table[fd] == NULL) {
		DEBUG(DB_FSYSCALL, "Write attempted on closed file %d.\n", fd);

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

	// TODO handle the initial file offset

	output.uio_offset = 0; // I think that this is where we're starting in the data to write, not the file
	output.uio_resid = nbytes; // how much data we can transfer
	output.uio_segflg = UIO_SYSSPACE;
	output.uio_rw = UIO_WRITE;
	output.uio_space = NULL; // the data is in kernel space

	// perform the actual write
	*err = VOP_WRITE(file_table[fd]->node, &output);
	int len = output.uio_offset;

	// free memory
	kfree(data);

	// TODO update the offset in the file

	if (*err == 0) {
		return len;
	} else {
		return -1;
	}

}
