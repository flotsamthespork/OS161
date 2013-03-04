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
	for (pid = 1; pid < MAX_PROCESSES; pid++) {
		if (runningprocesses[pid] == NULL) {
			break;
		}
	}

	if (pid == MAX_PROCESSES) {
		return EAGAIN;
	}

	*dst = kmalloc(sizeof(struct process));
	if (dst == NULL) {
		return ENOMEM;
	}

	(*dst)->p_exitcv = cv_create("");
	if ((*dst)->p_exitcv == NULL) {
		kfree(*dst);
		return ENOMEM;
	}

	(*dst)->p_exitlock = lock_create("");
	if ((*dst)->p_exitlock == NULL) {
		kfree((*dst)->p_exitcv);
		kfree(*dst);
		return ENOMEM;
	}

	// Keep track of the process
	runningprocesses[pid] = *dst;
	(*dst)->p_pid = pid;
	(*dst)->p_parentpid = curthread->t_pid;
	(*dst)->p_finished = 0;
	(*dst)->p_exitcode = 0;

	return 0;
}


void process_remove(pid_t pid) {
	struct process *process = runningprocesses[pid];
	assert(process != NULL);

	lock_destroy(process->p_exitlock);
	cv_destroy(process->p_exitcv);

	kfree(process);
	runningprocesses[pid] = NULL;
}


pid_t sys_fork(struct trapframe *tf, int *errorcode) {
	// TODO - possible disable interrupts
	int spl = splhigh();
	struct addrspace *newAddrspace = NULL;
	struct process *newProcess = NULL;
	struct thread *newThread = NULL;
	struct trapframe *newTrapFrame = kmalloc(sizeof(struct trapframe));
	pid_t pid = -1;

	if (newTrapFrame == NULL) {
		splx(spl);
		*errorcode = ENOMEM;
		return -1;
	}

	// Copy the address space for the current process
	*errorcode = as_copy(curthread->t_vmspace, &newAddrspace);
	if (*errorcode != 0) {
		kfree(newTrapFrame);
		splx(spl);
		return -1;
	}

	// Allocate the new trapframe, copy it and increment
	// the program counter.
	memcpy(newTrapFrame, tf, sizeof(struct trapframe));

	*errorcode = process_create(&newProcess);

	if (*errorcode != 0) {
		kfree(newTrapFrame);
		kfree(newAddrspace);
		splx(spl);
		return -1;
	}

	pid = newProcess->p_pid;


	// Actually fork to a new thread
	*errorcode = thread_fork("TODO - Thread Name",
			newAddrspace, (unsigned long) newTrapFrame,
			child_fork, &newThread);

	// If thread_fork has failed we want to leave immediately
	if (*errorcode != 0) {
		kfree(newTrapFrame);
		kfree(newAddrspace);
		process_remove(pid);
		splx(spl);
		return -1;
	}

	newThread->t_pid = pid;
	newProcess->p_thread = newThread;

	*errorcode = 0;
	splx(spl);
	return pid;

}
