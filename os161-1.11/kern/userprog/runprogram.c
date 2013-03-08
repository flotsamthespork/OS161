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

// void *argsToKernel(char **src, char ***dst, int argc) {
// 	int i, len, padlen;

// 	*dst = kmalloc(argc * sizeof(char *));
// 	if (*dst == NULL) {
// 		return NULL;
// 	}

// 	// copyin(const_userptr_t usersrc, void *dest, size_t len)
// 	for (i = 0; i < argc; i++) {
// 		len = 0;
// 		while (src[i][len] != '\0') {
// 			len += 1;
// 		}
// 		// Bring the padding length up to a multiple of 4
// 		padlen = len + (4 - (len % 4));

// 		*dst[i] = kmalloc(padlen * sizeof(char));
// 		if (dst == NULL) {
// 			// Free everything that has been kmalloc'd so far
// 			for (i -= 1; i >= 0; i--) {
// 				kfree(*dst[i]);
// 			}
// 			kfree(*dst);
// 			return NULL;
// 		}
// 		copyin(src[i], *dst[i], len);
// 		for (; len < padlen; len++) {
// 			*dst[len] = '\0';
// 		}
// 	}

// 	for (i = 0; i < argc; i++) {
// 		kprintf("SRC: %s, DST: %s", src[i], *dst[i]);
// 	}
// }

// void deleteKArgs(char ***ptr) {
// 	// TODO - delete that shit
// }

int ceilTo4(int value) {
	return (value + (4 - (value % 4)));
}

int getStrLength(char *ptr) {
	int len = 0;
	if (ptr != NULL) {
		for (len = 0; ptr[len] != '\0'; len += 1);
	}
	return len;
}

int copyArgsIn(char **src, char **dst, int argc) {
	int argSize, len, i, MEOW;
	const int ptrSize = sizeof(char *) * (argc + 1);
	char *arg;

	argSize = 0;
	for (i = 0; i < argc; i++) {
		arg = src[i];
		if (arg == NULL) {
			continue;
		}
		len = ceilTo4(getStrLength(arg));
		// kprintf("%s: %d\n", arg, len);
		argSize += len;
	}

	// kprintf("%d\n", totalSize);
	*dst = kmalloc(ptrSize + argSize);
	if (*dst == NULL) {
		return ENOMEM;
	}

	// copyinstr(const_userptr_t usersrc, char *dest, size_t len, size_t *actual)

	argSize = 0;

	// Copy in pointers first
	for (i = 0; i < argc; i++) {
		len = getStrLength(src[i]);
		MEOW = copyinstr(src[i], dst + (ptrSize + argSize), len, NULL);
		if (MEOW) {
			kprintf("WOOF %d\n", MEOW);
		}
		kprintf("NEWSTRING\n");
		for (MEOW = 0; MEOW < len; MEOW++) {
			kprintf("> %d\n", *dst[ptrSize + MEOW + argSize]);
		}
		// kprintf("%d: %s\n", (ptrSize + argSize), dst[(ptrSize + argSize)]);
		argSize += ceilTo4(len);
	}
}

/*
 * Load program "progname" and start running it in usermode.
 * Does not return except on error.
 *
 * Calls vfs_open on progname and thus may destroy it.
 */
int
runprogram(char *progname, int argc, char **argv)
{
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result;
	char *kargs = NULL;

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

	result = copyArgsIn(argv, &kargs, argc);
	if (result) {
		return result;
	}

	/* Warp to user mode. */
	md_usermode(0, NULL, //argv,
		    stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}

