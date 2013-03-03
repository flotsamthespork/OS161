#include <syscall.h>

#include <lib.h>
#include <fd.h>
#include <vfs.h>

int sys_open(const char *filename, int flags, int mode, int *err) {

	((void) mode); // hide unused argument warning

	int retval = -1;

	// TODO move this to process initialization
	if (!file_table_initialized) {
		initialize_file_table();
	}

	lock_acquire(file_table_lock);

	int i;
	for (i = FIRST_FILE_HANDLE; i < MAX_FILE_HANDLES; i++) {
		if (file_table[i] == NULL) {
			DEBUG(DB_FSYSCALL, "Opening %s with description %d\n", filename, i);

			// we found an empty entry so use this one
			file_table[i] = (struct fd *) kmalloc(sizeof(struct fd));

			file_table[i]->name = kstrdup(filename);
			file_table[i]->flags = flags;
			file_table[i]->position = 0;

			*err = vfs_open(file_table[i]->name, flags, &(file_table[i]->node));
			// TODO do we have to free path?

			DEBUG(DB_FSYSCALL, "vfs_open returned %d\n", *err);

			retval = i;
			break;
		}
	}

	lock_release(file_table_lock);

	return retval;

}
