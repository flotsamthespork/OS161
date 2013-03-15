#ifndef __PROCESS_H__
#define __PROCESS_H__

#define MAX_PROCESSES 128

#include <fd.h>
#include <thread.h>

struct process {
	pid_t p_pid;
	pid_t p_parentpid;
	struct thread *p_thread;
	int p_finished;
	int p_exitcode;
	struct cv* p_exitcv;
	struct cv* p_exitlock;

	struct lock *p_file_table_lock; // do I even need a lock here?
	struct fd *p_file_table[MAX_FILE_HANDLES];
};

int process_create(struct process **dst);

void process_remove(pid_t pid);

extern struct process *runningprocesses[];

#endif
