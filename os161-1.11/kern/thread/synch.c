/*
 * Synchronization primitives.
 * See synch.h for specifications of the functions.
 */

#include <types.h>
#include <lib.h>
#include <synch.h>
#include <thread.h>
#include <curthread.h>
#include <machine/spl.h>

#include <queue.h>
#include "opt-A1.h"

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *namearg, int initial_count)
{
	struct semaphore *sem;

	assert(initial_count >= 0);

	sem = kmalloc(sizeof(struct semaphore));
	if (sem == NULL) {
		return NULL;
	}

	sem->name = kstrdup(namearg);
	if (sem->name == NULL) {
		kfree(sem);
		return NULL;
	}

	sem->count = initial_count;
	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	spl = splhigh();
	assert(thread_hassleepers(sem)==0);
	splx(spl);

	/*
	 * Note: while someone could theoretically start sleeping on
	 * the semaphore after the above test but before we free it,
	 * if they're going to do that, they can just as easily wait
	 * a bit and start sleeping on the semaphore after it's been
	 * freed. Consequently, there's not a whole lot of point in 
	 * including the kfrees in the splhigh block, so we don't.
	 */

	kfree(sem->name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);

	/*
	 * May not block in an interrupt handler.
	 *
	 * For robustness, always check, even if we can actually
	 * complete the P without blocking.
	 */
	assert(in_interrupt==0);

	spl = splhigh();
	while (sem->count==0) {
		thread_sleep(sem);
	}
	assert(sem->count>0);
	sem->count--;
	splx(spl);
}

void
V(struct semaphore *sem)
{
	int spl;
	assert(sem != NULL);
	spl = splhigh();
	sem->count++;
	assert(sem->count>0);
	thread_wakeup(sem);
	splx(spl);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}

	lock->name = kstrdup(name);
	if (lock->name == NULL) {
		kfree(lock);
		return NULL;
	}

#if OPT_A1	
	lock->isLocked = 0;
	lock->thread = NULL;
#endif
	
	return lock;

}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

#if OPT_A1
	lock->thread = NULL;
#endif
	
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
#if OPT_A1
	int spl;

	assert(lock != NULL);
	while (lock->isLocked) {
		thread_yield();
	}

	spl = splhigh();

	lock->isLocked = 1;
	lock->thread = curthread;

	splx(spl);

#else
	(void)lock;  // suppress warning until code gets written
#endif
}

void
lock_release(struct lock *lock)
{
#if OPT_A1
	int spl;
	assert(lock != NULL);

	spl = splhigh();

	// Only release the lock if it is currently locked and
	// the currently running thread is the one that locked it.
	if (lock->isLocked && lock->thread == curthread) {
		lock->thread = NULL;
		lock->isLocked = 0;
	}

	splx(spl);

#else
	(void)lock;  // suppress warning until code gets written
#endif
}

int
lock_do_i_hold(struct lock *lock)
{
#if OPT_A1
	assert (lock != NULL);

	return (lock->isLocked && lock->thread == curthread ? 1 : 0);
#else
	(void)lock;  // suppress warning until code gets written

	return 1;    // dummy until code gets written
#endif
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->name = kstrdup(name);
	if (cv->name==NULL) {
		kfree(cv);
		return NULL;
	}
	
#if OPT_A1
	cv->queue = q_create(1);
	if (cv->queue == NULL) {
		kfree(cv);
		return NULL;
	}
#endif
	
	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

#if OPT_A1
	q_destroy(cv->queue);
	cv->queue = NULL;
#endif
	
	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
#if OPT_A1
	int spl;

	assert(cv != NULL);
	assert(lock != NULL);
	
	lock_release(lock);
	spl = splhigh();
	q_addtail(cv->queue, curthread);
	thread_sleep(curthread);
	splx(spl);
	lock_acquire(lock);
#else
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
#endif
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
#if OPT_A1
	int spl;

	assert(cv != NULL);
	assert(lock != NULL);

	spl = splhigh();

	if (!q_empty(cv->queue)) {
		thread_wakeup(q_remhead(cv->queue));
	}

	splx(spl);

#else
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
#endif
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
#if OPT_A1
	int spl;

	assert(cv != NULL);
	assert(lock != NULL);

	spl = splhigh();

	while (!q_empty(cv->queue)) {
		cv_signal(cv, lock);
	}

	splx(spl);

#else
	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written
#endif
}
