#include <assert.h>
#include <stdlib.h>
#include <ucontext.h>
#include "thread.h"
#include "interrupt.h"

typedef enum states{
	RUNNING = 1,
	READY = 2,
	EXITED = 3,
	WAITING = 4
} tState;

/* This is the thread control block */
typedef struct thread {
	/* ... Fill this in ... */
	Tid threadId;
	tState threadState;
	ucontext_t threadContext;
	void* threadStack;
} thread;

typedef struct threadNode {
	thread* thread;
	struct threadNode* next;
} threadNode;

/* This is the wait queue structure */
struct wait_queue {
	/* ... Fill this in Lab 3 ... */
	threadNode* queue;
};

// function prototypes
void thread_stub(void (*thread_main)(void *), void *arg);
threadNode* newNode(thread* thread);
threadNode* queuePop(threadNode* queue, threadNode** element);
threadNode* queueRemove(threadNode* queue, Tid threadId, threadNode** element);
threadNode* queuePush(threadNode* queue, threadNode* element);
void emptyQueue(threadNode* queue);
void printQueue(threadNode* queue);
Tid thread_switch(Tid next_thread, threadNode** destinationQueue, tState newState);

// global data structures
int currThreadCount;
threadNode* readyQueue;
threadNode* runningQueue;
threadNode* exitedQueue;
thread* allThreads[THREAD_MAX_THREADS];
struct wait_queue* allWaitQueues[THREAD_MAX_THREADS];

// data structure/queue functions
threadNode* newNode(thread* thread) {
	threadNode* newNode = malloc(sizeof(threadNode));

	if (newNode != NULL) {
		newNode -> thread = thread;
		newNode -> next = NULL;
	}
	
	return newNode;
}

threadNode* queuePop(threadNode* queue, threadNode** element) {
	if (queue == NULL) return queue;

	threadNode* newHead = queue -> next;
	queue -> next = NULL;
	*element = queue;
	return newHead;
}

threadNode* queueRemove(threadNode* queue, Tid threadId, threadNode** element) {
	if (queue != NULL) {
		threadNode* curr = queue;

		if (curr -> thread -> threadId == threadId) {
			threadNode* newHead = curr -> next;
			curr -> next = NULL;
			*element = curr;
			return newHead;
		}

		while (curr -> next != NULL && curr -> next -> thread -> threadId != threadId) {
			curr = curr -> next;
		}

		if (curr -> next != NULL) {
			threadNode* foundThread = curr -> next;
			curr -> next = foundThread -> next;
			foundThread -> next = NULL;
			*element = foundThread;
		}
	} 

	return queue;
}

threadNode* queuePush(threadNode* queue, threadNode* element) {
	if (queue == NULL) return element;
	else {
		threadNode* curr = queue;

		while (curr -> next != NULL) curr = curr -> next;

		curr -> next = element;
		return queue;
	}

	return queue;
}

void emptyQueue(threadNode* queue) {
	if (queue == NULL) return;
	else if (queue -> next != NULL) emptyQueue(queue -> next);

	queue -> next = NULL;
	Tid currId = queue -> thread -> threadId;
	void* currStack = queue -> thread -> threadStack;
	allThreads[queue -> thread -> threadId] = NULL;
	free(queue -> thread);
	free(queue);
	if (currId != 0) free(currStack);

	return;
}

void printQueue(threadNode* queue) {
	threadNode* curr = queue;

	while (curr != NULL) {
		printf("%d", curr -> thread -> threadId);
		printf(" -> ");
		curr = curr -> next;
	}

	printf("\nEnd of Queue\n");
}

// thread functions

/* thread starts by calling thread_stub. The arguments to thread_stub are the
 * thread_main() function, and one argument to the thread_main() function. */
void thread_stub(void (*thread_main)(void *), void *arg) {
	interrupts_on();
	thread_main(arg); // call thread_main() function with arg
	thread_exit();
	return;
}

void thread_init(void) {
	// lock this function via disabling interrupts
	int prevState = interrupts_off();

	currThreadCount = 0;
	readyQueue = NULL;
	runningQueue = NULL;
	exitedQueue = NULL;

	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
		allThreads[i] = NULL;
		allWaitQueues[0] = NULL;
	}

	// initialize currently running thread with Tid = 0
	thread* currThread = malloc(sizeof(thread));
	allThreads[0] = currThread;
	allWaitQueues[0] = wait_queue_create();

	currThread -> threadId = 0;
	int err = getcontext(&(currThread -> threadContext));
	assert(!err);
	currThread -> threadStack = (void*) (currThread -> threadContext).uc_mcontext.gregs[REG_RSP];
	currThread -> threadState = RUNNING; 

	threadNode* currThreadNode = malloc(sizeof(threadNode));
	currThreadNode -> thread = currThread;
	currThreadNode -> next = NULL;

	// save the current thread to the running queue as it is currently running
	runningQueue = queuePush(runningQueue, currThreadNode);
	currThreadCount++;

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

Tid thread_id() {	
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	Tid currThread;
	if (runningQueue == NULL) currThread =  THREAD_INVALID;
	else currThread = runningQueue -> thread -> threadId;

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return currThread;
}

Tid thread_create(void (*fn) (void *), void *parg) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// before running the main code of the function, empty all exited queues to free up Tids and memory
	emptyQueue(exitedQueue); 
	exitedQueue = NULL;

	// allocate space for thread structures for new thread
	thread* newThread = malloc(sizeof(thread));

	if (newThread == NULL) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_NOMEMORY;
	}

	threadNode* newThreadNode = newNode(newThread);

	if (newThreadNode == NULL) {
		free(newThread);
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_NOMEMORY;
	}

	// find a TID
	Tid threadId = THREAD_MAX_THREADS;
	for (int i = 0; i < THREAD_MAX_THREADS; i++) {
		if (allThreads[i] == NULL) {
			threadId = i;
			break;
		}
	}

	if (threadId == THREAD_MAX_THREADS) {
		free(newThread);
		free(newThreadNode);
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_NOMORE;
	}

	// set up the basic info for the new thread
	newThread -> threadState = READY;
	newThread -> threadId = threadId;

	// allocate the stack for the thread
	void* newStack = malloc(THREAD_MIN_STACK);

	if (newStack == NULL) {
		free(newThread);
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_NOMEMORY;
	}

	newThread -> threadStack = newStack;

	// set up the context for the new thread
	int err = getcontext(&(newThread -> threadContext));
	assert(!err);

	// set up PC for new thread (starts at the thread_stub function) as well as registers for input arguments
	(newThread -> threadContext).uc_mcontext.gregs[REG_RIP] = (long long int) &thread_stub;
	(newThread -> threadContext).uc_mcontext.gregs[REG_RDI] = (long long int) fn;
	(newThread -> threadContext).uc_mcontext.gregs[REG_RSI] = (long long int) parg;

	// set up stack pointer for the new thread (needs to start at top and be aligned to 16)
	// initialize frame pointer to the same start location as stack pointer
	(newThread -> threadContext).uc_mcontext.gregs[REG_RSP] = ((long long int) newStack + THREAD_MIN_STACK) - (((long long int) newStack + THREAD_MIN_STACK) % 16) - 8;
	(newThread -> threadContext).uc_mcontext.gregs[REG_RBP] = (newThread -> threadContext).uc_mcontext.gregs[REG_RSP];

	// save the new thread in the array and the ready queue
	readyQueue = queuePush(readyQueue, newThreadNode);
	allThreads[threadId] = newThread;
	currThreadCount++;

	// initialize wait queue for this thread
	allWaitQueues[threadId] = wait_queue_create();

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return threadId;
}

Tid thread_yield(Tid want_tid) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// before running the main code of the function, empty all exited queues to free up Tids and memory
	emptyQueue(exitedQueue);
	exitedQueue = NULL; 

	Tid switchedThread;

	if (want_tid < -2 || want_tid >= THREAD_MAX_THREADS) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_INVALID;
	} else if (want_tid == THREAD_ANY) {
		// switch to first thread in ready queue, if there are any
		if (readyQueue == NULL) {
			// re-enable interrupts before returning
			interrupts_set(prevState);
			return THREAD_NONE;
		} else {
			Tid newThread = readyQueue -> thread -> threadId;
			switchedThread = thread_switch(newThread, &readyQueue, READY);
		}
	} else if (want_tid == THREAD_SELF) {
		// switch to itself
		switchedThread = thread_switch(runningQueue -> thread -> threadId, &readyQueue, READY);
	} else {
		// check if want_tid exists
		if (allThreads[want_tid] == NULL) {
			// re-enable interrupts before returning
			interrupts_set(prevState);
			return THREAD_INVALID;
		}
		// switch to want_tid
		switchedThread = thread_switch(want_tid, &readyQueue, READY);
	}

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return switchedThread;
}

void thread_exit() {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// wake up any threads that were waiting on this one
	// then destroy the wait queue since it will no longer be needed
	thread_wakeup(allWaitQueues[runningQueue -> thread -> threadId], 1);
	wait_queue_destroy(allWaitQueues[runningQueue -> thread -> threadId]);
	allWaitQueues[runningQueue -> thread -> threadId] = NULL;

	// if there are other threads, switch to one
	// else, free up all memory and then exit program
	if (readyQueue != NULL) {
		// figure out what thread to switch to, and switch to it
		Tid want_tid = readyQueue -> thread -> threadId; 
		currThreadCount--;
		thread_switch(want_tid, &exitedQueue, EXITED);
	} else {
		// should do nothing since those queues should all be empty
		emptyQueue(readyQueue);
		emptyQueue(exitedQueue);

		// empty the current thread
		void* currStack = runningQueue -> thread -> threadStack;
		free(runningQueue -> thread);
		free(runningQueue);
		free(currStack);

		// re-enable interrupts before returning
		interrupts_set(prevState);
		exit(0);
	}
	
	// should never reach here
	return;
}

Tid thread_kill(Tid tid) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// first check for valid input
	if (tid < 0 || tid >= THREAD_MAX_THREADS) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_INVALID;
	}
	else if (allThreads[tid] == NULL || allThreads[tid] -> threadState == EXITED) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		 return THREAD_INVALID;
	}
	else if (tid == runningQueue -> thread -> threadId) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_INVALID;
	}

	// if input is valid then mark it as exited
	// if the thread is in the ready queue, also remove the thread from the ready queue and move it to the exited queue
	else {
		if (allThreads[tid] -> threadState == READY) {
			threadNode* toKill = NULL;
			readyQueue = queueRemove(readyQueue, tid, &toKill);
			exitedQueue = queuePush(exitedQueue, toKill);
		}
		
		allThreads[tid] -> threadState = EXITED;

		// wake up any threads that were waiting on this one
		// then destroy the wait queue since it will no longer be needed
		thread_wakeup(allWaitQueues[tid], 1);
		wait_queue_destroy(allWaitQueues[tid]);
		allWaitQueues[tid] = NULL;
		currThreadCount--;
	}

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return tid;
}

Tid thread_switch(Tid next_thread, threadNode** destinationQueue, tState newState) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	threadNode* currThreadNode = NULL;
	runningQueue = queuePop(runningQueue, &currThreadNode);

	// update state for current thread
	currThreadNode -> thread -> threadState = newState;
	*destinationQueue = queuePush(*destinationQueue, currThreadNode);

	// find and update state for next thread
	threadNode* nextThreadNode = NULL;
	readyQueue = queueRemove(readyQueue, next_thread, &nextThreadNode);
	nextThreadNode -> thread -> threadState = RUNNING;
	runningQueue = queuePush(runningQueue, nextThreadNode);

	// swapping the threads of execution
	volatile int setcontext_called = 0;
	int err = getcontext(&(currThreadNode -> thread -> threadContext));
	assert(!err);

	if (setcontext_called == 1) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return next_thread;
	}

	setcontext_called = 1;
	setcontext(&(nextThreadNode -> thread -> threadContext));

	// function should never reach here!
	// re-enable interrupts before returning
	interrupts_set(prevState);
	return next_thread;
}

/*******************************************************************
 * Important: The rest of the code should be implemented in Lab 3. *
 *******************************************************************/

/* make sure to fill the wait_queue structure defined above */
struct wait_queue* wait_queue_create() {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// allocate the wait queue structure
	struct wait_queue *wq;

	wq = malloc(sizeof(struct wait_queue));
	assert(wq);

	// initialize the wait queue structure
	wq -> queue = NULL;

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return wq;
}

void wait_queue_destroy(struct wait_queue* wq) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// if some threads are still in the wait queue, wake them up first
	if (wq -> queue != NULL) thread_wakeup(wq, 1);

	// actually free the wait queue structure
	free(wq);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

Tid thread_sleep(struct wait_queue* queue) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// don't put the current thread to sleep if the wait queue is invalid
	// or if there are no ready threads that can start running

	if (queue == NULL) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_INVALID;
	}

	if (readyQueue == NULL) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_NONE;
	}

	Tid newThread = readyQueue -> thread -> threadId;
	Tid switched_thread = thread_switch(newThread, &(queue -> queue), WAITING);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return switched_thread;
}

/* when the 'all' parameter is 1, wakeup all threads waiting in the queue.
 * returns whether a thread was woken up on not. */
int thread_wakeup(struct wait_queue* queue, int all) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// don't wakeup any threads if the queue is invalid or empty
	if (queue == NULL) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return 0;
	} else if (queue -> queue == NULL) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return 0;
	}

	// wakeup only one thread if all == 0
	if (all == 0) {
		threadNode* wokenThreadNode = NULL;
		queue -> queue = queuePop(queue -> queue, &wokenThreadNode);
		if (wokenThreadNode -> thread -> threadState == EXITED) exitedQueue = queuePush(exitedQueue, wokenThreadNode);
		else readyQueue = queuePush(readyQueue, wokenThreadNode);
		
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return 1;
	} 

	// wakeup all threads if all == 1
	if (all == 1) {
		int numWoken = 0;
		while (queue -> queue != NULL) {
			threadNode* wokenThreadNode = NULL;
			queue -> queue = queuePop(queue -> queue, &wokenThreadNode);
			if (wokenThreadNode -> thread -> threadState == EXITED) exitedQueue = queuePush(exitedQueue, wokenThreadNode);
			else readyQueue = queuePush(readyQueue, wokenThreadNode);
			numWoken++;
		}

		// re-enable interrupts before returning
		interrupts_set(prevState);
		return numWoken;
	}

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return 0;
}

/* suspend current thread until Thread tid exits */
Tid thread_wait(Tid tid) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	// check for valid input
	if (tid < 0 || tid >= THREAD_MAX_THREADS || tid == runningQueue -> thread -> threadId || allThreads[tid] == NULL) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return THREAD_INVALID;
	}

	thread_sleep(allWaitQueues[tid]);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return tid;
}

struct lock {
	/* ... Fill this in ... */
	int status;
	Tid acquirer;
	struct wait_queue* queue;
};

struct lock* lock_create() {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	struct lock *lock;

	lock = malloc(sizeof(struct lock));
	assert(lock);

	lock -> status = 0;
	lock -> acquirer = -1;
	lock -> queue = wait_queue_create();

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return lock;
}

void lock_destroy(struct lock* lock) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	assert(lock != NULL);
	assert(lock -> status == 0);

	wait_queue_destroy(lock -> queue);

	free(lock);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

void lock_acquire(struct lock* lock) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	assert(lock != NULL);

	while (lock -> status == 1) {
		thread_sleep(lock -> queue);
	}

	lock -> status = 1;
	lock -> acquirer = runningQueue -> thread -> threadId;

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

void lock_release(struct lock* lock) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	assert(lock != NULL);

	// can't release a lock if its not acquired by the currently running thread
	if (lock -> acquirer != runningQueue -> thread -> threadId) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return;
	}

	lock -> status = 0;
	lock -> acquirer = -1;

	// wake up any waiting threads
	thread_wakeup(lock -> queue, 1);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

struct cv {
	/* ... Fill this in ... */
	struct wait_queue* queue;
};

struct cv* cv_create() {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	struct cv *cv;

	cv = malloc(sizeof(struct cv));
	assert(cv);

	cv -> queue = wait_queue_create();

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return cv;
}

void cv_destroy(struct cv* cv) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	assert(cv != NULL);

	// only free the condition variable if its wait queue is empty
	if (cv -> queue -> queue != NULL) {
		// re-enable interrupts before returning
		interrupts_set(prevState);
		return;
	}

	wait_queue_destroy(cv -> queue);
	free(cv);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

void cv_wait(struct cv* cv, struct lock* lock) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();

	assert(cv != NULL);
	assert(lock != NULL);

	// make sure that current thread has acquired the lock, or else acquire it
	if (lock -> acquirer != runningQueue -> thread -> threadId) {
		lock_acquire(lock);
	}

	lock_release(lock);
	thread_sleep(cv -> queue);
	lock_acquire(lock);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

void cv_signal(struct cv* cv, struct lock* lock) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();
	assert(cv != NULL);
	assert(lock != NULL);

	// make sure that current thread has acquired the lock, or else acquire it
	if (lock -> acquirer != runningQueue -> thread -> threadId) {
		lock_acquire(lock);
	}

	// wake up a single thread waiting for the CV
	thread_wakeup(cv -> queue, 0);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}

void cv_broadcast(struct cv* cv, struct lock* lock) {
	// disable interrupt to lock this function
	int prevState = interrupts_off();
	assert(cv != NULL);
	assert(lock != NULL);

	// make sure that current thread has acquired the lock, or else acquire it
	if (lock -> acquirer != runningQueue -> thread -> threadId) {
		lock_acquire(lock);
	}

	// wake up all threads waiting for the CV
	thread_wakeup(cv -> queue, 1);

	// re-enable interrupts before returning
	interrupts_set(prevState);
	return;
}
