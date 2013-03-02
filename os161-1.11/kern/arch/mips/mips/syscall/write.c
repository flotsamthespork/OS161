#include <syscall.h>
#include <lib.h>
#include <unistd.h>

int sys_write(int fd, const void *buf, size_t nbytes) {

	// TODO make this use actual devices/files
	// TODO don't use kprintf for console printing
	if (fd != STDOUT_FILENO && fd != STDERR_FILENO) {
		kprintf("Printing to stdout instead of a file");
	}
	kprintf((char *) buf);

	return -1;

}
