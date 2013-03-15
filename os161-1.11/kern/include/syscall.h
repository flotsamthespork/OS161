#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <types.h>

struct fd;
struct trapframe;

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);

int sys_open(const char *filename, int flags, int mode, int *err);
int _open(struct fd *file_table[], int fd, const char *filename, int flags, int mode);
int sys_close(int fd, int *err);
void _close(struct fd *file_table[], int fd);
int sys_read(int fd, void *buf, size_t buflen, int *err);
int sys_write(int fd, const void *buf, size_t nbytes, int *err);

pid_t sys_fork(struct trapframe *tf, int *errorcode);
pid_t sys_getpid();
pid_t sys_waitpid(pid_t pid, int *status, int options, int *errorcode);
void sys__exit(int exitcode);
int sys_execv(const char *program, char **args);


#endif /* _SYSCALL_H_ */
