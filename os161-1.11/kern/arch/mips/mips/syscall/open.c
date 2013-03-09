#include <syscall.h>

#include <lib.h>
#include <fd.h>
#include <vfs.h>

int sys_open(const char *filename, int flags, int mode, int *err) {

	int retval = -1;

	// TODO move this to process initialization
	initialize_file_table_if_necessary();

	lock_acquire(file_table_lock);

	int i;
	for (i = FIRST_FILE_HANDLE; i < MAX_FILE_HANDLES; i++) {
		if (file_table[i] == NULL) {
			// TODO do we need to free the duplicated filename?
			*err = _open(i, kstrdup(filename), flags, mode);

//			DEBUG(DB_FSYSCALL, "Opening %s with description %d\n", filename, i);
//
//			// we found an empty entry so use this one
//			file_table[i] = (struct fd *) kmalloc(sizeof(struct fd));
//
//			file_table[i]->name = kstrdup(filename);
//			file_table[i]->flags = flags;
//			file_table[i]->position = 0;
//
//			*err = vfs_open(file_table[i]->name, flags, &(file_table[i]->node));
//			// TODO do we have to free path?
//
//			DEBUG(DB_FSYSCALL, "vfs_open returned %d\n", *err);

			retval = i;
			break;
		}
	}

	lock_release(file_table_lock);

	return retval;

}

/** Performs the actual opening given a file handle so that we can use this
 * to open stdio automatically from read/write. **/
int _open(int fd, char *filename, int flags, int mode) {

	((void) mode); // hide unused argument warning

	// we can assert here since the kernel is the only thing calling us directly
	assert(fd >= 0 && fd < MAX_FILE_HANDLES);
	assert(file_table[fd] == NULL);

	file_table[fd] = (struct fd *) kmalloc(sizeof(struct fd));

	file_table[fd]->name = kstrdup(filename);
	file_table[fd]->flags = flags;
	file_table[fd]->position = 0;

	int err = vfs_open(file_table[fd]->name, file_table[fd]->flags, &(file_table[fd]->node));

	DEBUG(DB_FSYSCALL, "vfs_open returned %d when opening %s as %d in %d\n", err, file_table[fd]->name, file_table[fd]->flags, fd);

	return err;
}
