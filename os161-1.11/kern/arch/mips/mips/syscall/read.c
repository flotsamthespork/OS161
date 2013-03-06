#include <syscall.h>

#include <fd.h>
#include <vfs.h>
#include <uio.h>
#include <vnode.h>
#include <lib.h>
#include <kern/unistd.h>
#include <kern/errno.h>

int sys_read(int fd, void *buf, size_t buflen, int *err) {

	// make sure that we're using a file handle which is actually open
	if (fd < 0 || fd > MAX_FILE_HANDLES - 1) {
		DEBUG(DB_FSYSCALL, "Invalid file handle %d\n", fd);

		*err = EBADF;
		return -1;
	}

	// handle the special cases (stdin, stdout, and stderr)
	if (fd == 0) {
		if (file_table[fd] == NULL) {
			DEBUG(DB_FSYSCALL, "Opening stdin because it isn't open yet.\n");

			*err = _open(fd, "con:", O_RDONLY, 0);

			if (*err != 0) {
				DEBUG(DB_FSYSCALL, "Error opening stdin for read.\n");

				return -1;
			}
		}
	} else if (fd == 1 || fd == 2) {
		DEBUG(DB_FSYSCALL, "Read attempted from %s.\n", (fd == 1 ? "stdout" : "stderr"));

		*err = EBADF;
		return -1;
	}

	// make sure that the file is open
	if (file_table[fd] == NULL) {
		DEBUG(DB_FSYSCALL, "Read attempted on closed file %d.\n", fd);

		*err = EBADF;
		return -1;
	}

	struct uio output;

	// create a buffer in kernel space which we'll copy back into user space
	void *data = kmalloc(buflen);

	// set where the data is and how long it is
	output.uio_iovec.iov_kbase = data;
	output.uio_iovec.iov_len = buflen;

	output.uio_offset = 0; // I think that this is where we're starting in the data to write, not the file
	output.uio_resid = buflen; // how much data we can transfer
	output.uio_segflg = UIO_SYSSPACE;
	output.uio_rw = UIO_READ;
	output.uio_space = NULL; // the data is in kernel space

	// perform the actual read
	*err = VOP_READ(file_table[fd]->node, &output);
	int len = output.uio_offset;

	// copy the memory back to user space and then free
	memcpy(buf, data, len);
	kfree(data);

	if (*err == 0) {
		return len;
	} else {
		return -1;
	}

}
