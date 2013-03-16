#include <syscall.h>
#include <addrspace.h>
#include <curthread.h>
#include <thread.h>
#include <process.h>
#include <synch.h>
#include <kern/errno.h>
#include <machine/trapframe.h>
#include <lib.h>


struct process *runningprocesses[MAX_PROCESSES];


/**
 * Entry function for the new process.
 */
void child_fork(void *asPtr, unsigned long tfPtr) {
	struct addrspace *as = (struct addrspace*) asPtr;
	struct trapframe *tf = (struct trapframe*) tfPtr;

	// Increment the program counter
	tf->tf_epc += 4;

	tf->tf_v0 = 0; // PID = 0 for child thread
	tf->tf_a3 = 0; // No error

	curthread->t_vmspace = as;

	as_activate(as);

	struct trapframe newTrapFrame;
	memcpy(&newTrapFrame, tf, sizeof(struct trapframe));
	kfree(tf);

	mips_usermode(&newTrapFrame);
}

int process_create(struct process **dst) {
	int pid;
	lock_acquire(process_lock);
	for (pid = 1; pid < MAX_PROCESSES; pid++) {
		if (runningprocesses[pid] == NULL) {
			break;
		}
	}

	if (pid == MAX_PROCESSES) {
		lock_release(process_lock);
		return EAGAIN;
	}

	return process_create_for_id(pid, dst, process_lock);
}

int process_create_for_id(pid_t pid, struct process **dst, struct lock *p_lock) {
	*dst = kmalloc(sizeof(struct process));
	if (dst == NULL) {
		if (p_lock != NULL) {
			lock_release(p_lock);
		}
		return ENOMEM;
	}

	(*dst)->p_exitcv = cv_create("");
	if ((*dst)->p_exitcv == NULL) {
		kfree(*dst);
		if (p_lock != NULL) {
			lock_release(p_lock);
		}
		return ENOMEM;
	}

	(*dst)->p_exitlock = lock_create("");
	if ((*dst)->p_exitlock == NULL) {
		kfree((*dst)->p_exitcv);
		kfree(*dst);
		if (p_lock != NULL) {
			lock_release(p_lock);
		}
		return ENOMEM;
	}

	// Keep track of the process
	runningprocesses[pid] = *dst;
	(*dst)->p_pid = pid;
	(*dst)->p_parentpid = curthread->t_pid;
	(*dst)->p_finished = 0;
	(*dst)->p_exitcode = 0;

	if (p_lock != NULL) {
		lock_release(p_lock);
	}

	// initialize the file table and its lock
	(*dst)->p_file_table_lock = lock_create("file_table_lock");
	if ((*dst)->p_file_table_lock == NULL) {
		lock_destroy((*dst)->p_exitlock);
		cv_destroy((*dst)->p_exitcv);
		kfree(*dst);

		return ENOMEM;
	}

	int i;
	for (i = 0; i < MAX_FILE_HANDLES; i++) {
		(*dst)->p_file_table[i] = NULL;
	}

	return 0;
}


void process_remove(pid_t pid) {
	struct process *process = runningprocesses[pid];
	assert(process != NULL);

	lock_acquire(process_lock);

	lock_destroy(process->p_exitlock);
	cv_destroy(process->p_exitcv);

	// destroy the file table
	lock_destroy(process->p_file_table_lock);

	// close unclosed files
	int i;
	for (i = FIRST_FILE_HANDLE; i < MAX_FILE_HANDLES; i++) {
		if (process->p_file_table[i] != NULL) {
			_close(process->p_file_table, i);
		}
	}

	kfree(process);
	runningprocesses[pid] = NULL;

	lock_release(process_lock);
}


pid_t sys_fork(struct trapframe *tf, int *errorcode) {
	struct addrspace *newAddrspace = NULL;
	struct process *newProcess = NULL;
	struct thread *newThread = NULL;
	struct trapframe *newTrapFrame = kmalloc(sizeof(struct trapframe));
	pid_t pid = -1;

	if (newTrapFrame == NULL) {
		*errorcode = ENOMEM;
		return -1;
	}

	// Copy the address space for the current process
	*errorcode = as_copy(curthread->t_vmspace, &newAddrspace);
	if (*errorcode != 0) {
		kfree(newTrapFrame);
		return -1;
	}

	// Allocate the new trapframe, copy it and increment
	// the program counter.
	memcpy(newTrapFrame, tf, sizeof(struct trapframe));

	*errorcode = process_create(&newProcess);

	if (*errorcode != 0) {
		kfree(newTrapFrame);
		kfree(newAddrspace);
		return -1;
	}

	pid = newProcess->p_pid;

	lock_acquire(process_lock);

	// copy the file table
	struct fd **parent_file_table = runningprocesses[curthread->t_pid]->p_file_table;
	struct fd **child_file_table = newProcess->p_file_table;

	int i;
	for (i = 0; i < MAX_FILE_HANDLES; i++) {
		if (parent_file_table[i] != NULL) {
			*errorcode = _open(child_file_table, i, parent_file_table[i]->name, parent_file_table[i]->flags, 0);

			// if we couldn't duplicate an entry, then there's no point in duplicating the rest
			if (*errorcode != 0) {
				break;
			}
		}
	}

	if (*errorcode != 0) {
		for (i = FIRST_FILE_HANDLE; i < MAX_FILE_HANDLES; i++) {
			if (child_file_table[i] != NULL) {
				_close(child_file_table, i);
			}
		}

		lock_release(process_lock);

		kfree(newTrapFrame);
		kfree(newAddrspace);
		process_remove(pid);

		return -1;
	}
	lock_release(process_lock);

	// Actually fork to a new thread
	*errorcode = thread_fork("TODO - Thread Name",
			newAddrspace, (unsigned long) newTrapFrame,
			child_fork, &newThread);

	// If thread_fork has failed we want to leave immediately
	if (*errorcode != 0) {
		kfree(newTrapFrame);
		kfree(newAddrspace);
		process_remove(pid);
		return -1;
	}

	newThread->t_pid = pid;
	newProcess->p_thread = newThread;

	*errorcode = 0;
	return pid;

}
