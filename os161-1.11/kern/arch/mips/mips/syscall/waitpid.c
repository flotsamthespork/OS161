#include <syscall.h>
#include <process.h>
#include <curthread.h>
#include <synch.h>
#include <kern/errno.h>

pid_t sys_waitpid(pid_t pid, int *status, int options, int *errcode) {
	if (options != 0) {
		*errcode = EINVAL;
		return -1;
	}
	if (pid < 1 || pid >= MAX_PROCESSES) {
		// TODO - invalid pid
		*errcode = EFAULT;
		return -1;
	}
	struct process *process = runningprocesses[pid];

	if (process == NULL) {
		*errcode = EFAULT;
		return -1;
	}

	if (process->p_parentpid != curthread->t_pid) {
		*errcode = EFAULT;
		return -1;
	}

	lock_acquire(process->p_exitlock);
	while (!process->p_finished) {
		cv_wait(process->p_exitcv, process->p_exitlock);
	}

	*status = process->p_exitcode;
	lock_release(process->p_exitlock);
	process_remove(pid);
	*errcode = 0;
	return pid;

}
