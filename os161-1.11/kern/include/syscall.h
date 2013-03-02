#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <unistd.h>
#include <sys/wait.h>

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

//int sys_open(const char *filename, int flags);
int sys_open(const char *filename, int flags, int mode);
int sys_close(int fd);
int sys_read(int fd, void *buf, size_t buflen);
int sys_write(int fd, const void *buf, size_t nbytes);

pid_t sys_fork();
pid_t sys_getpid();
pid_t sys_waitpid(pid_t pid, int *status, int options);
void sys__exit(int exitcode);
int sys_execv(const char *program, char **args);


#endif /* _SYSCALL_H_ */
