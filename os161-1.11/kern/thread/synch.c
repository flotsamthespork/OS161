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

	// add stuff here as needed
	lock->owner = NULL;

	lock->queue = q_create(LOCK_QUEUE_INITIAL_SIZE);
	assert(lock->queue != NULL);

#endif

	return lock;
}

void
lock_destroy(struct lock *lock)
{
	assert(lock != NULL);

#if OPT_A1

	int spl = splhigh();

	// make sure that no one has this lock
	assert(lock->owner == NULL);

	// and that there's no one waiting on this lock
	assert(q_empty((struct queue *) lock->queue));

	splx(spl);

#endif
	
	kfree(lock->name);
	kfree(lock);
}

void
lock_acquire(struct lock *lock)
{

#if OPT_A1

	// make sure I don't already hold this lock
	assert(lock->owner != curthread);

	int spl = splhigh();

	// wait until no one is using the lock
	while (lock->owner != NULL) {
		q_addtail((struct queue *) lock->queue, curthread);
		thread_sleep(curthread);
	}

	// give me the lock
	lock->owner = curthread;

	splx(spl);


#else

	(void)lock;  // suppress warning until code gets written

#endif

}

void
lock_release(struct lock *lock)
{

#if OPT_A1

	// make sure I actually hold this lock
	assert(lock_do_i_hold(lock));

	int spl = splhigh();

	// give up the lock
	lock->owner = NULL;

	// wake up the first waiting thread, if there is anything waiting
	if (!q_empty((struct queue *) lock->queue))
	{
		thread_wakeup(q_remhead((struct queue *) lock->queue));
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

	return lock->owner == curthread;

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

	// create the queue
	cv->queue = q_create(CV_QUEUE_INITIAL_SIZE);
	assert(cv->queue != NULL);

#endif

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	assert(cv != NULL);

#if OPT_A1

	int spl = splhigh();

	// make sure there's nothing waiting on us
	assert(q_empty((struct queue *) cv->queue));

	// destroy the queue
	q_destroy((struct queue *) cv->queue);

	splx(spl);

#endif

	kfree(cv->name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{

#if OPT_A1

	// make sure I actually hold the lock
	assert(lock_do_i_hold(lock));

	int spl = splhigh();

	// re-release the lock
	lock_release(lock);

	// add to the cv's queue and then sleep the thread on itself while it is waiting
	q_addtail((struct queue *) cv->queue, curthread);
	thread_sleep(curthread);

	// re-acquire the lock
	lock_acquire(lock);

	splx(spl);

#else

	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written

#endif

}

void
cv_signal(struct cv *cv, struct lock *lock)
{

#if OPT_A1

	// make sure I hold the lock
	assert(lock_do_i_hold(lock));

	int spl = splhigh();

	// release the lock
	lock_release(lock);

	// wake up the first waiting thread, if there is anything waiting
	if (!q_empty((struct queue *) cv->queue))
	{
		thread_wakeup(q_remhead((struct queue *) cv->queue));
	}

	// re-acquire the lock
	lock_acquire(lock);

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

	// make sure I hold the lock
	assert(lock_do_i_hold(lock));

	int spl = splhigh();

	// release the lock
	lock_release(lock);

	// wake up every waiting thread
	while (!q_empty((struct queue *) cv->queue))
	{
		thread_wakeup(q_remhead((struct queue *) cv->queue));
	}

	// re-acquire the lock
	lock_acquire(lock);

	splx(spl);

#else

	(void)cv;    // suppress warning until code gets written
	(void)lock;  // suppress warning until code gets written

#endif

}
