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

int sys_execv(const char *program, char **args) {
	struct vnode *v;
	vaddr_t entrypoint, stackptr;
	int result, i;
	char **kargs, sargs;

	// Get the argument count
	int argc = 0;
	if (args != NULL) {
		while (args[argc] != NULL) {
			argc += 1;
		}
	}

	/* Open the file. */
	result = vfs_open(program, O_RDONLY, &v);
	if (result) {
		return result;
	}

	// Destroy old address space
	if (curthread->t_vmspace != NULL) {
		as_destroy(curthread->t_vmspace);
		curthread->t_vmspace = NULL;
	}

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

	for (i = 0; i < argc; i++) {
		
	}

	/* Warp to user mode. */
	md_usermode(argc, NULL /*userspace addr of argv*/,
		    stackptr, entrypoint);
	
	/* md_usermode does not return */
	panic("md_usermode returned\n");
	return EINVAL;
}
