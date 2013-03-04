#include <syscall.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>
#include <process.h>
#include <synch.h>

void sys__exit(int exitcode) {
	int spl = splhigh();

	// kprintf("Exiting... pid(%d)\n", curthread->t_pid);

	struct process *process = runningprocesses[curthread->t_pid];
	if (process != NULL) {
		lock_acquire(process->p_exitlock);
		process->p_finished = 1;
		process->p_exitcode = exitcode;
		cv_broadcast(process->p_exitcv, process->p_exitlock);
		lock_release(process->p_exitlock);
	}

	splx(spl);

	thread_exit();

	// TODO

}
