#include <syscall.h>
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

int getArgCount(char **args, int *argc) {
	int error;
	*argc = 0;
	if (args == NULL) {
		return EFAULT; // Is the actually even an error?
	}

	error = as_valid_ptr(args);
	if (error) {
		return error;
	}

	// TODO - SMASH ANY BAD ARGUMENTS WITH THE POWER OF ERRORS

	for (; args[*argc] != NULL; *argc += 1) {
		error = as_valid_ptr(args[*argc]);
		if (error) {
			return error;
		}
	}
	// Return no errors
	return 0;
}




// Brings the given value up to the next multiple of 4.
// Eg. 0 -> ceilTo4 -> 4,
//     5 -> ceilTo4 -> 8
static int ceilTo4(int value) {
	return (value + (4 - (value % 4)));
}




static int copyArgsIn(char **src, char **dst, int argc, int *totalsize) {
	int i, len, argsize, error;

	const int ptrsize = sizeof(char *) * (argc + 1);

	argsize = 0;
	if (src != NULL) {
		for (i = 0; i < argc; i++) {
			if (src[i] != NULL) {
				argsize += ceilTo4(strlen(src[i]));
			} else {
				// TODO - why the fuck is there a null argument?
			}
		}
	}

	*totalsize = ptrsize + argsize;

	*dst = kmalloc(*totalsize);
	if (*dst == NULL) {
		return ENOMEM;
	}

	argsize = 0;

	for (i = 0; i <= argc; i++) {
		// Check that we are still in the bounds of the
		// input and the input isn't null
		if (i < argc && src != NULL && src[i] != NULL) {
			len = strlen(src[i]);

			int error = copyinstr(src[i], *dst + ptrsize + argsize, len + 1, NULL);
			if (error) {
				kfree(*dst);
				*dst = NULL;
				return;
			}

			((int*)*(dst))[i] = ptrsize + argsize;

			argsize += ceilTo4(len);
		} else {
			((int*)*(dst))[i] = 0;
		}
	}
	
	return 0;
}




static int copyArgsOut(char *src, char *dst, int argc, int argsize) {
	int i, dataoffset = 4*(argc+1);
	int *ptrdst = (int*) dst;
	int *ptrsrc = (int*) src;
	for (i = 0; i < argc; i++) {
		ptrdst[i] = dst + ptrsrc[i];
	}
	ptrdst[argc] = NULL;
	int error = copyout(src + dataoffset,
						dst + dataoffset,
						argsize - dataoffset);
	if (error) {
		return error;
	}
	return 0;
}




int sys_execv(const char *program, char **args) {
		struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, argc, argsize;
	char *kargs;

	if (program == NULL) {
		return EFAULT;
	}

	result = as_valid_ptr(program);
	if (result) {
		return result;
	}

	if (strlen(program) == 0) {
		return EINVAL;
	}

	result = getArgCount(args, &argc);
	if (result) {
		return result;
	}

	result = copyArgsIn(args, &kargs, argc, &argsize);
	if (result) {
		return result;
	}

	/* Open the file. */
	result = vfs_open(program, O_RDONLY, &v);
	if (result) {
		kfree(kargs);
		return result;
	}

	// Destroy the old address space
	as_destroy(curthread->t_vmspace);
	curthread->t_vmspace = NULL;


	/* We should be a new thread. */
	assert(curthread->t_vmspace == NULL);

	/* Create a new address space. */
	curthread->t_vmspace = as_create();
	if (curthread->t_vmspace==NULL) {
		vfs_close(v);
		kfree(kargs);
		return ENOMEM;
	}

	/* Activate it. */
	as_activate(curthread->t_vmspace);

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		kfree(kargs);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		kfree(kargs);
		return result;
	}

	// TODO - copy the arguments from kbuffer to stack
	result = copyArgsOut(kargs, stackptr-argsize-8, argc, argsize);
	if (result) {
		kfree(kargs);
		return result;
	}

	/* Warp to user mode. */
	md_usermode(argc, (stackptr - argsize - 8), //argv,
		    (stackptr - argsize - 12), entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	kfree(kargs);
	return EINVAL;
}
