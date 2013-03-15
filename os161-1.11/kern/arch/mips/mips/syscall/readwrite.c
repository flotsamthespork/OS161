#include <syscall.h>

#include <curthread.h>
#include <fd.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <lib.h>
#include <process.h>
#include <synch.h>
#include <uio.h>
#include <vfs.h>
#include <vnode.h>

static struct uio *constructUio(enum uio_rw operation, void *buffer, size_t length, size_t file_offset);

int sys_read(int fd, void *buf, size_t buflen, int *err) {

	DEBUG(DB_FSYSCALL, "Reading from file handle %d in process %d\n", fd, curthread->t_pid);

	struct lock *file_table_lock = runningprocesses[curthread->t_pid]->p_file_table_lock;
	struct fd **file_table = runningprocesses[curthread->t_pid]->p_file_table;

	// check arguments
	if (as_valid_ptr(buf) != 0) { // check that the buffer is a valid ptr
		DEBUG(DB_FSYSCALL, "Invalid buffer %x given for reading.\n", buf);

		*err = EFAULT;
		goto error;
	} else if (fd < 0 || fd > MAX_FILE_HANDLES - 1) { // check that the file handle is valid
		DEBUG(DB_FSYSCALL, "Invalid file handle %d\n", fd);

		*err = EBADF;
		goto error;
	} else if (fd == 1 || fd == 2) { // check that we aren't trying to read from stdout or stderr
		DEBUG(DB_FSYSCALL, "Read attempted from %s.\n", (fd == 1 ? "stdout" : "stderr"));

		*err = EBADF;
		goto error;
	}

	// if we're reading from stdin, make sure it's open
	if (fd == 0 && file_table[fd] == NULL) {
		DEBUG(DB_FSYSCALL, "Opening stdin because it isn't open yet.\n");

		*err = _open(file_table, fd, "con:", O_RDONLY, 0);

		if (*err != 0) goto error;
	}

	lock_acquire(file_table_lock);

	// check that the file is readable
	if (file_table[fd] == NULL) { // check that the file is actually open
		DEBUG(DB_FSYSCALL, "Read attempted on closed file %d.\n", fd);

		*err = EBADF;
		goto error;
	} else if (file_table[fd]->flags == O_WRONLY) { // check that the file isn't write-only
		DEBUG(DB_FSYSCALL, "Read attempted on write-only file %d.\n", fd);

		*err = EBADF;
		goto error;
	}

	// prepare for input
	struct uio *input = constructUio(UIO_READ, buf, buflen, file_table[fd]->position);

	// perform the actual read
	*err = VOP_READ(file_table[fd]->node, input);
	int length = input->uio_offset - file_table[fd]->position;

	// get rid of the input
	kfree(input);

	if (*err == 0) {
		file_table[fd]->position += length;

		lock_release(file_table_lock);

		return length;
	} else {
		goto error;
	}

error:

	if (lock_do_i_hold(file_table_lock)) {
		lock_release(file_table_lock);
	}

	return -1;

}

int sys_write(int fd, const void *buf, size_t nbytes, int *err) {

	DEBUG(DB_FSYSCALL, "Writing to file handle %d in process %d\n", fd, curthread->t_pid);

	struct lock *file_table_lock = runningprocesses[curthread->t_pid]->p_file_table_lock;
	struct fd **file_table = runningprocesses[curthread->t_pid]->p_file_table;

	// check arguments
	if (as_valid_ptr(buf) != 0) { // check that the buffer is a valid ptr
		DEBUG(DB_FSYSCALL, "Invalid buffer %x given for reading.\n", buf);

		*err = EFAULT;
		goto error;
	} else if (fd < 0 || fd > MAX_FILE_HANDLES - 1) { // check that the file handle is valid
		DEBUG(DB_FSYSCALL, "Invalid file handle %d\n", fd);

		*err = EBADF;
		goto error;
	} else if (fd == 0) { // check that we're not trying to write to stdin
		DEBUG(DB_FSYSCALL, "Write attempted from stdin.\n");

		*err = EBADF;
		goto error;
	}

	// if we're reading from stdout or stderr, make sure it's open
	if ((fd == 1 || fd == 2) && file_table[fd] == NULL) {
		DEBUG(DB_FSYSCALL, "Opening %s because it isn't open yet.\n", (fd == 1 ? "stdout" : "stderr"));

		*err = _open(file_table, fd, "con:", O_WRONLY, 0);

		if (*err != 0) goto error;
	}

	lock_acquire(file_table_lock);

	// check that the file is writeable
	if (file_table[fd] == NULL) { // check that the file is actually open
		DEBUG(DB_FSYSCALL, "Write attempted on closed file %d.\n", fd);

		*err = EBADF;
		goto error;
	} else if (file_table[fd]->flags == O_RDONLY) { // check that the file isn't read-only
		DEBUG(DB_FSYSCALL, "Write attempted on read-only file %d.\n", fd);

		*err = EBADF;
		goto error;
	}

	// prepare for output
	struct uio *output = constructUio(UIO_WRITE, buf, nbytes, file_table[fd]->position);

	// perform the actual write
	*err = VOP_WRITE(file_table[fd]->node, output);
	int length = output->uio_offset - file_table[fd]->position;

	// get rid of the output
	kfree(output);

	if (*err == 0) {
		file_table[fd]->position += length;

		lock_release(file_table_lock);

		return length;
	} else {
		goto error;
	}

error:

	if (lock_do_i_hold(file_table_lock)) {
		lock_release(file_table_lock);
	}

	return -1;

}

static struct uio *constructUio(enum uio_rw operation, void *buffer, size_t length, size_t file_offset) {
	struct uio *ret = kmalloc(sizeof(struct uio));

	ret->uio_iovec.iov_kbase = buffer;
	ret->uio_iovec.iov_len = length;

	ret->uio_offset = file_offset;
	ret->uio_resid = length;
	ret->uio_segflg = UIO_USERSPACE;
	ret->uio_rw = operation;
	ret->uio_space = curthread->t_vmspace;

	return ret;
}

