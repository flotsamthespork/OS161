#include <addrspace.h>
#include <curthread.h>
#include <fd.h>
#include <lib.h>
#include <process.h>
#include <synch.h>
#include <syscall.h>
#include <thread.h>

void sys__exit(int exitcode) {

	lock_acquire(process_lock);

	struct process *process = runningprocesses[curthread->t_pid];
	if (process != NULL) {

		// close any unclosed files
		lock_acquire(process->p_file_table_lock);

		struct fd **file_table = process->p_file_table;
		int i;
		for (i = FIRST_FILE_HANDLE; i < MAX_FILE_HANDLES; i++) {
			if (file_table[i] != NULL) {
				_close(file_table, i);
			}
		}

		lock_release(process->p_file_table_lock);

		lock_acquire(process->p_exitlock);
		process->p_finished = 1;
		process->p_exitcode = exitcode;
		cv_broadcast(process->p_exitcv, process->p_exitlock);
		lock_release(process->p_exitlock);
	}

	lock_release(process_lock);

	thread_exit();

	// TODO

}
