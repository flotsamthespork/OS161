/*
 * catmouse.c
 *
 * 30-1-2003 : GWA : Stub functions created for CS161 Asst1.
 * 26-11-2007: KMS : Modified to use cat_eat and mouse_eat
 * 21-04-2009: KMS : modified to use cat_sleep and mouse_sleep
 * 21-04-2009: KMS : added sem_destroy of CatMouseWait
 * 05-01-2012: TBB : added comments to try to clarify use/non use of volatile
 *
 */


/*
 * 
 * Includes
 *
 */

#include <types.h>
#include <lib.h>
#include <test.h>
#include <thread.h>
#include <synch.h>

#include <queue.h>

/*
 * 
 * cat,mouse,bowl simulation functions defined in bowls.c
 *
 * For Assignment 1, you should use these functions to
 *  make your cats and mice eat from the bowls.
 * 
 * You may *not* modify these functions in any way.
 * They are implemented in a separate file (bowls.c) to remind
 * you that you should not change them.
 *
 * For information about the behaviour and return values
 *  of these functions, see bowls.c
 *
 */

/* this must be called before any calls to cat_eat or mouse_eat */
extern int initialize_bowls(unsigned int bowlcount);

extern void cleanup_bowls( void );
extern void cat_eat(unsigned int bowlnumber);
extern void mouse_eat(unsigned int bowlnumber);
extern void cat_sleep(void);
extern void mouse_sleep(void);


void initialize_manager(void);
void cleanup_manager(void);
void swap_turns();
void free_bowl(unsigned int bowlnumber);
int get_bowl(unsigned int type);
void leave_bowl(unsigned int bowlnumber);

// Constants for each type of animal
const unsigned int TYPE_NULL = 0;
const unsigned int TYPE_CAT = 1;
const unsigned int TYPE_MOUSE = 2;


volatile unsigned int currentTurn;
volatile unsigned int isSwapping;
volatile unsigned int numServed;
volatile unsigned int numEating;
volatile unsigned int numWaitingForBowl;
volatile unsigned int numWaitingForTurn;

// Locks multiple animals from trying to find
// a bowl at the same time
struct lock *bowlLock;

struct cv *bowlCv;

struct queue *bowlQueue;


/*
 *
 * Problem parameters
 *
 * Values for these parameters are set by the main driver
 *  function, catmouse(), based on the problem parameters
 *  that are passed in from the kernel menu command or
 *  kernel command line.
 *
 * Once they have been set, you probably shouldn't be
 *  changing them.
 *
 * These are only ever modified by one thread, at creation time,
 * so they do not need to be volatile.
 */
int NumBowls;  // number of food bowls
int NumCats;   // number of cats
int NumMice;   // number of mice
int NumLoops;  // number of times each cat and mouse should eat

/*
 * Once the main driver function (catmouse()) has created the cat and mouse
 * simulation threads, it uses this semaphore to block until all of the
 * cat and mouse simulations are finished.
 */
struct semaphore *CatMouseWait;

/*
 * 
 * Function Definitions
 * 
 */


/*
 * cat_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long catnumber: holds cat identifier from 0 to NumCats-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      Each cat simulation thread runs this function.
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 */

static
void
cat_simulation(void * unusedpointer, 
               unsigned long catnumber)
{
  int i;
  unsigned int bowl;

  /* avoid unused variable warnings. */
  (void) unusedpointer;
  (void) catnumber;


  /* your simulated cat must iterate NumLoops times,
   *  sleeping (by calling cat_sleep() and eating
   *  (by calling cat_eat()) on each iteration */
  for(i=0;i<NumLoops;i++) {

    /* do not synchronize calls to cat_sleep().
       Any number of cats (and mice) should be able
       sleep at the same time. */
    cat_sleep();

    /* for now, this cat chooses a random bowl from
     * which to eat, and it is not synchronized with
     * other cats and mice.
     *
     * you will probably want to control which bowl this
     * cat eats from, and you will need to provide 
     * synchronization so that the cat does not violate
     * the rules when it eats */

    /* legal bowl numbers range from 1 to NumBowls */
    bowl = get_bowl(TYPE_CAT);
    cat_eat(bowl);
    leave_bowl(bowl);

  }

  /* indicate that this cat simulation is finished */
  V(CatMouseWait); 
}

/*
 * mouse_simulation()
 *
 * Arguments:
 *      void * unusedpointer: currently unused.
 *      unsigned long mousenumber: holds mouse identifier from 0 to NumMice-1.
 *
 * Returns:
 *      nothing.
 *
 * Notes:
 *      each mouse simulation thread runs this function
 *
 *      You need to finish implementing this function using 
 *      the OS161 synchronization primitives as indicated
 *      in the assignment description
 *
 */

static
void
mouse_simulation(void * unusedpointer,
          unsigned long mousenumber)
{
  int i;
  unsigned int bowl;

  /* Avoid unused variable warnings. */
  (void) unusedpointer;
  (void) mousenumber;


  /* your simulated mouse must iterate NumLoops times,
   *  sleeping (by calling mouse_sleep()) and eating
   *  (by calling mouse_eat()) on each iteration */
  for(i=0;i<NumLoops;i++) {

    /* do not synchronize calls to mouse_sleep().
       Any number of mice (and cats) should be able
       sleep at the same time. */
    mouse_sleep();

    /* for now, this mouse chooses a random bowl from
     * which to eat, and it is not synchronized with
     * other cats and mice.
     *
     * you will probably want to control which bowl this
     * mouse eats from, and you will need to provide 
     * synchronization so that the mouse does not violate
     * the rules when it eats */

    /* legal bowl numbers range from 1 to NumBowls */
    bowl = get_bowl(TYPE_MOUSE);
    mouse_eat(bowl);
    leave_bowl(bowl);

  }

  /* indicate that this mouse is finished */
  V(CatMouseWait); 
}


/*
 * catmouse()
 *
 * Arguments:
 *      int nargs: should be 5
 *      char ** args: args[1] = number of food bowls
 *                    args[2] = number of cats
 *                    args[3] = number of mice
 *                    args[4] = number of loops
 *
 * Returns:
 *      0 on success.
 *
 * Notes:
 *      Driver code to start up cat_simulation() and
 *      mouse_simulation() threads.
 *      You may need to modify this function, e.g., to
 *      initialize synchronization primitives used
 *      by the cat and mouse threads.
 *      
 *      However, you should should ensure that this function
 *      continues to create the appropriate numbers of
 *      cat and mouse threads, to initialize the simulation,
 *      and to wait for all cats and mice to finish.
 */

int
catmouse(int nargs,
         char ** args)
{
  int index, error;
  int i;

  /* check and process command line arguments */
  if (nargs != 5) {
    kprintf("Usage: <command> NUM_BOWLS NUM_CATS NUM_MICE NUM_LOOPS\n");
    return 1;  // return failure indication
  }

  /* check the problem parameters, and set the global variables */
  NumBowls = atoi(args[1]);
  if (NumBowls <= 0) {
    kprintf("catmouse: invalid number of bowls: %d\n",NumBowls);
    return 1;
  }
  NumCats = atoi(args[2]);
  if (NumCats < 0) {
    kprintf("catmouse: invalid number of cats: %d\n",NumCats);
    return 1;
  }
  NumMice = atoi(args[3]);
  if (NumMice < 0) {
    kprintf("catmouse: invalid number of mice: %d\n",NumMice);
    return 1;
  }
  NumLoops = atoi(args[4]);
  if (NumLoops <= 0) {
    kprintf("catmouse: invalid number of loops: %d\n",NumLoops);
    return 1;
  }
  kprintf("Using %d bowls, %d cats, and %d mice. Looping %d times.\n",
          NumBowls,NumCats,NumMice,NumLoops);

  /* create the semaphore that is used to make the main thread
     wait for all of the cats and mice to finish */
  CatMouseWait = sem_create("CatMouseWait",0);
  if (CatMouseWait == NULL) {
    panic("catmouse: could not create semaphore\n");
  }

  /* 
   * initialize the bowls
   */
  if (initialize_bowls(NumBowls)) {
    panic("catmouse: error initializing bowls.\n");
  }

  initialize_manager();

  /*
   * Start NumCats cat_simulation() threads.
   */
  for (index = 0; index < NumCats; index++) {
    error = thread_fork("cat_simulation thread",NULL,index,cat_simulation,NULL);
    if (error) {
      panic("cat_simulation: thread_fork failed: %s\n", strerror(error));
    }
  }

  /*
   * Start NumMice mouse_simulation() threads.
   */
  for (index = 0; index < NumMice; index++) {
    error = thread_fork("mouse_simulation thread",NULL,index,mouse_simulation,NULL);
    if (error) {
      panic("mouse_simulation: thread_fork failed: %s\n",strerror(error));
    }
  }

  /* wait for all of the cats and mice to finish before
     terminating */  
  for(i=0;i<(NumCats+NumMice);i++) {
    P(CatMouseWait);
  }

  /* clean up the semaphore that we created */
  sem_destroy(CatMouseWait);

  /* clean up resources used for tracking bowl use */
  cleanup_bowls();

  cleanup_manager();

  return 0;
}

/*
 * End of catmouse.c
 */


void initialize_manager() {
	int i;
	currentTurn = TYPE_NULL;

	isSwapping = 0;
	numServed = 0;
	numEating = 0;
	numWaitingForBowl = 0;
	numWaitingForTurn = 0;

	bowlLock = lock_create("bowl_lock");
	assert(bowlLock != NULL);

	bowlCv = cv_create("bowl_cv");
	assert(bowlCv != NULL);

	bowlQueue = q_create(NumBowls);
	assert(bowlQueue != NULL);

	for (i = 1; i <= NumBowls; ++i) {
		free_bowl(i);
	}
}

void cleanup_manager() {
	lock_destroy(bowlLock);
	cv_destroy(bowlCv);
	while (!q_empty(bowlQueue)) {
		kfree(q_remhead(bowlQueue));
	}
	q_destroy(bowlQueue);
}


void swap_turns() {
	int temp = numWaitingForBowl;
	numWaitingForBowl = numWaitingForTurn;
	numWaitingForTurn = temp;
	numServed = 0;
	isSwapping = 1;

	if (currentTurn == TYPE_MOUSE) {
		currentTurn = TYPE_CAT;
	}
	else {
		currentTurn = TYPE_MOUSE;
	}
}


int get_bowl(const unsigned int type) {
	int *bowlnumberptr;
	int bowlnumber;
	lock_acquire(bowlLock);

	if (currentTurn == TYPE_NULL) {
		currentTurn = type;
	}
	
	if (currentTurn == type) {
		numWaitingForBowl += 1;
	}
	else {
		numWaitingForTurn += 1;
	}

	while (isSwapping || currentTurn != type || q_empty(bowlQueue)) {
		cv_wait(bowlCv, bowlLock);

		if (!isSwapping && numWaitingForTurn > numWaitingForBowl ||
				numServed > 2 * NumBowls) {
			swap_turns();
		}

		if (numEating == 0) {
			isSwapping = 0;
		}
	}

	// Get the next queued bowl number and frees the pointer
	// to the value in it
	bowlnumberptr = (int*)q_remhead(bowlQueue);
	bowlnumber = *bowlnumberptr;
	kfree(bowlnumberptr);

	numWaitingForBowl -= 1;
	numEating += 1;

	lock_release(bowlLock);

	return bowlnumber;
}


void free_bowl(const unsigned int bowlnumber) {
	int *ptr = kmalloc(sizeof(int));
	*ptr = bowlnumber;
	q_addtail(bowlQueue, ptr);

	cv_broadcast(bowlCv, bowlLock);
}


void leave_bowl(const unsigned int bowlnumber) {

	numServed += 1;
	numEating -= 1;

	if (numWaitingForTurn == 0) {
		numServed = 0;
	}

	free_bowl(bowlnumber);
}

