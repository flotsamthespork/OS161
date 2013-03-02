#include <syscall.h>
#include <lib.h>
#include <thread.h>
#include <curthread.h>
#include <addrspace.h>

void sys__exit(int exitcode) {

	kprintf("Exiting...\n");

	thread_exit();

	// TODO

}
