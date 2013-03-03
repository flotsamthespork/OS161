#ifndef _FD_H_
#define _FD_H_

#include <types.h>
#include <synch.h>
#include <lib.h> // TODO remove this line

#define MAX_FILE_HANDLES 32

#define FIRST_FILE_HANDLE 3

struct fd {
	char *name;
	int flags;
	off_t position;
	struct vnode *node;
};

// TODO move this to the process
struct lock *file_table_lock;
struct fd *file_table[MAX_FILE_HANDLES];

int file_table_initialized = 0;

void initialize_file_table() {
	DEBUG(DB_FSYSCALL, "Initializing file table\n");

	file_table_lock = lock_create("file_table_lock");

	lock_acquire(file_table_lock);

	int i;
	for (i = 0; i < MAX_FILE_HANDLES; i++) {
		file_table[i] = NULL;
	}

	file_table_initialized = 1;

	lock_release(file_table_lock);
}

#endif
