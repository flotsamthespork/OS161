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
	int valid;
};

// TODO move this to the process
extern struct lock *file_table_lock;
extern struct fd *file_table[MAX_FILE_HANDLES];

void initialize_file_table_if_necessary();

#endif
