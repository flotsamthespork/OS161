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


// Counts the number of arguments detected in "args".
// Returns 0 if there wasn't an error with argument counting.
int getArgCount(char **args, int *argc) {
	int error;
	*argc = 0;

	// Check that the argument array exists
	if (args == NULL) {
		return EFAULT; 
	}

	// Check that the argument array is a valid pointer
	// in the address space of the program.
	error = as_valid_ptr((vaddr_t)args);
	if (error) {
		return error;
	}

	// Count arguments until the terminating NULL is found
	for (; args[*argc] != NULL; *argc += 1) {
		// Check that each argument is not an invalid pointer
		error = as_valid_ptr((vaddr_t)args[*argc]);
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




// Copys the arguments array from src and puts it in kernel in a collapsed
// structure, dst. Offset corresponds to the size of this new structure.
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
// After running the algorithm, dst will look like this:
//
// *(dst+0) = 3
// *(dst+1) = 2
// *(dst+2) = 1
// *(dst+3) = NULL
// *(dst+4) = (dst+0)
// *(dst+5) = (dst+1)
// *(dst+6) = (dst+2)
//
// Returns 0 on success
static int copyArgsIn(char **src, char **dst, int argc, int *totalsize) {
	int i, len, argsize, error;

	// Size taken up by the pointers in dst
	const int ptrsize = sizeof(char *) * (argc + 1);

	// Calculate the size required to store the arguments in dst
	argsize = 0;
	for (i = 0; i < argc; i++) {
		// Round each size up to the nearest 4 so their address
		// will be a valid pointer
		argsize += ceilTo4(strlen(src[i]));
	}

	// Calculate the total size of dst and kmalloc it
	*totalsize = ptrsize + argsize;
	*dst = kmalloc(*totalsize);
	if (*dst == NULL) {
		return ENOMEM;
	}

	// Copy arguments into kernel
	argsize = 0;
	for (i = 0; i < argc; i++) {
		// Get the length of the argument so we know how much we
		// need to copy into dst.
		len = strlen(src[i]);

		error = copyinstr((void*)src[i], // Address of the argument
									  // from src
			(char*)(*dst + ptrsize + argsize), // offset address of the
									  // argument in dst
			len + 1, // Add 1 to length to copy terminating character
			NULL);
		if (error) {
			kfree(*dst);
			*dst = NULL;
			return error;
		}

		// Evaluate the value of the argument pointer relative to dst
		((int*)*(dst))[i] = ptrsize + argsize;

		// Update the offset into the argument array
		argsize += ceilTo4(len);
	}
	
	return 0;
}




// Copies the data structure (in dst), as shown in copyArgsIn,
// into user space so it may be used by the program being
// executed.
static int copyArgsOut(char *src, char *dst, int argc, int argsize) {
	int i;
	// See copyArgsIn
	const int ptrsize = sizeof(char *) * (argc + 1);

	// Interpret dst/src as char** so we can simply set the
	// values of the pointers (this way ptrdst[i] corresponds
	// to a 4-byte offset into the pointer instead of 1 byte)
	char **ptrdst = (char**) dst;
	char **ptrsrc = (char**) src;
	for (i = 0; i < argc; i++) {
		// Add the offset calculated in copyArgsIn onto dst
		// to get the actual address for the pointer
		ptrdst[i] = dst + (int)ptrsrc[i];
	}
	// Set the NULL pointer
	ptrdst[argc] = NULL;


	int error = copyout(
		// Copy everything beyond the pointers
		(void*)(src + ptrsize),
		// Place everything beyond the pointers in user space
		(void*)(dst + ptrsize),
		// Copy the total size of the arguments only
		(size_t)(argsize - ptrsize));
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
	char *progname;

	if (program == NULL) {
		return EFAULT;
	}

	result = as_valid_ptr((vaddr_t)program);
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

	progname = kstrdup(program);
	if (progname == NULL) {
		kfree(kargs);
		return ENOMEM;
	}

	/* Open the file. */
	result = vfs_open(progname, O_RDONLY, &v);
	if (result) {
		kfree(kargs);
		kfree(progname);
		return result;
	}

	kfree(progname);

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

	curthread->t_vmspace->as_v = v;

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		vfs_close(v);
		kfree(kargs);
		return result;
	}

	/* Define the user stack in the address space */
	result = as_define_stack(curthread->t_vmspace, &stackptr);
	if (result) {
		/* thread_exit destroys curthread->t_vmspace */
		kfree(kargs);
		vfs_close(v);
		return result;
	}

	result = copyArgsOut(kargs, (char*)(stackptr-argsize-8), argc, argsize);
	kfree(kargs);
	if (result) {
		vfs_close(v);
		return result;
	}

	/* Warp to user mode. */
	md_usermode(argc, (userptr_t)(stackptr - argsize - 8), //argv,
		    (vaddr_t)(stackptr - argsize - 12), entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}
