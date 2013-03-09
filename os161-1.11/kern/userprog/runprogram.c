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
#include <curthread.h>
#include <vm.h>
#include <vfs.h>
#include <test.h>


#if OPT_A2

// Brings the given value up to the next multiple of 4.
// Eg. 0 -> ceilTo4 -> 4,
//     5 -> ceilTo4 -> 8
int ceilTo4(int value) {
	return (value + (4 - (value % 4)));
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
int copyArgsOut(char **src, char *dst, int argc, int *offset) {
	int argSize, totalSize, len, i, error;

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
		if (src[i] != NULL) {
			// If the pointer isn't null we probably have a string
			len = strlen(src[i]);

			*ptr = ((int)dst) - totalSize + ptrSize + argSize;

			// Try to copy the string into user space
			error = copyoutstr(src[i], (void*)*ptr, len + 1, NULL);
			if (error) {
				kfree(ptr);
				return error;
			}

			// Try to copy the pointer into user space
			error = copyout(ptr, dst - totalSize + (i*4), 4);
			if (error) {
				kfree(ptr);
				return error;
			}

			argSize += ceilTo4(len);;
		}
		else {
			// Null pointer
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
	result = copyArgsOut(argv, (char *) stackptr - 4, argc, &offset);
	if (result) {
		return result;
	}

	/* Warp to user mode. */
	md_usermode(argc, (stackptr - offset - 4), //argv,
		    (stackptr - offset - 8), entrypoint);
#else
	/* Warp to user mode. */
	md_usermode(0, NULL,stackptr, entrypoint);
#endif
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

