#include <syscall.h>
#include <curthread.h>

pid_t sys_getpid() {
	return curthread->t_pid;
}
