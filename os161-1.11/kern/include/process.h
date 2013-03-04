#ifndef __PROCESS_H__
#define __PROCESS_H__

#define MAX_PROCESSES 256

#include <thread.h>

struct process {
	pid_t p_pid;
	pid_t p_parentpid;
	struct thread *p_thread;
	int p_finished;
	int p_exitcode;
	struct cv* p_exitcv;
	struct cv* p_exitlock;
};

int process_create(struct process **dst);

void process_remove(pid_t pid);

extern struct process *runningprocesses[];

#endif
