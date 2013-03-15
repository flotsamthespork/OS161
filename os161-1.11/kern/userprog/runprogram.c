/*
 * Sample/test code for running a user program.  You can use this for
 * reference when implementing the execv() system call. Remember though
 * that execv() needs to do more than this function does.
 */

#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <thread.h>
#include <process.h>
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>


#if OPT_A2

// Brings the given value up to the next multiple of 4.
// Eg. 0 -> ceilTo4 -> 4,
//     5 -> ceilTo4 -> 8
static int ceilTo4(int value) {
	return (value + (4 - (value % 4)));
}

// Returns the length of the string passed in
int getStrLength(char *ptr) {
	int len = 0;
	if (ptr != NULL) {
		for (len = 0; ptr[len] != '\0'; len += 1);
	}
	return len;
}

// Copys the arguments array from src and puts it in a collapsed
// structure on dst. Offset corresponds to the size of this new structure.
//
// Eg.
// *(src+0) = ptr1;
// *(src+1) = ptr2;
// *(src+2) = ptr3;
// *(src+3) = NULL;
// *ptr1 = 1
// *ptr2 = 2
// *ptr3 = 3
//
// After runing the algorithm, dst will look like this:
//
// *(dst+0) = 3
// *(dst+1) = 2
// *(dst+2) = 1
// *(dst+3) = NULL
// *(dst+4) = (dst+0)
// *(dst+5) = (dst+1)
// *(dst+6) = (dst+2)
static int copyArgsOut(char **src, char *dst, int argc, int *offset) {
	int argSize, totalSize, len, padlen, i, error;

	// The size for the pointers in dst is 1 more than argc
	// because we need to store NULL at the end
	const int ptrSize = sizeof(char *) * (argc + 1);

	const int *nullPtr = NULL;

	// Pointer to the location in dst to copy data
	int *ptr = kmalloc(sizeof(int));

	// Figure out the total size of dst
	argSize = 0;
	for (i = 0; i < argc; i++) {
		// Ignore null pointers
		if (src[i] == NULL) {
			continue;
		}
		len = ceilTo4(strlen(src[i]));
		argSize += len;
	}

	totalSize = ptrSize + argSize;

	argSize = 0;

	for (i = 0; i <= argc; i++) {
		// Check that we are still in the bounds of the
		// input and the input isn't null
		if (i < argc && src[i] != NULL) {
			len = strlen(src[i]);
			padlen = ceilTo4(len);

			*ptr = ((int)dst) - totalSize + ptrSize + argSize;

			// Try to copy the string into dst
			error = copyoutstr(src[i], (void*)*ptr, len + 1, NULL);
			if (error) {
				kfree(ptr);
				return error;
			}

			// Try to copy the pointer into dst
			error = copyout(ptr, dst - totalSize + (i*4), 4);
			if (error) {
				kfree(ptr);
				return error;
			}

			argSize += padlen;
		} else {
			// Try to copy NULL into the arguments array
			error = copyout(&nullPtr, dst - totalSize + (i*4), 4);
			if (error) {
				kfree(ptr);
				return error;
			}
		}
	}


	kfree(ptr);

	*offset = totalSize;
	return 0;
}

#endif

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
#if OPT_A2
runprogram(char *progname, int argc, char **argv)
#else
runprogram(char *progname)
#endif
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, offset;

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		return result;
	}

	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		return result;
	}

#if OPT_A2

	/* Get us a process */
	struct process *proc;
	int err = process_create(&proc);

	if (err != 0) {
		// TODO handle me
		kprintf("process_create FAILED IN RUNPROGRAM! OH DEAR LAWD!\n");
	} else {
		curthread->t_pid = proc->p_pid;
	}

	/* Copy arguments */
	result = copyArgsOut(argv, (char *) stackptr - 8, argc, &offset);
	if (result) {
		return result;
	}

	/* Warp to user mode. */
	md_usermode(argc, (stackptr - offset - 8), //argv,
		    (stackptr - offset - 12), entrypoint);
#else
	/* Warp to user mode. */
	md_usermode(0, NULL,stackptr, entrypoint);
#endif
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

