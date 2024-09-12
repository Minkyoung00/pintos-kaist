#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
// #include "devices/timer.h"
#ifdef VM
#include "vm/vm.h"
#endif

#define USERPROG

/* States in a thread's life cycle. */
enum thread_status {
	THREAD_RUNNING,     /* Running thread. */
	THREAD_READY,       /* Not running but ready to run. */
	THREAD_BLOCKED,     /* Waiting for an event to trigger. */
	THREAD_DYING        /* About to be destroyed. */
};

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */


// 소수 연산 매크로 생성
#define F (1 << 14)  // 고정 소수점 비율 정의

#define FLOAT(n) ((n) * F)  // 정수를 고정 소수점으로 변환
#define INT(n) ((n) / F)  // 고정 소수점을 정수로 변환

#define ROUNDINT(x) (((x) >= 0) ? (((x) + F / 2) / F) : (((x) - F / 2) / F))  // 반올림하여 정수로 변환

#define ADDFI(x, i) ((x) + (i) * F)  // 고정 소수점에 정수 추가
#define SUBIF(i, f) ((i) * F - (f))  // 정수에서 고정 소수점 뺌

#define MUL(x, y) (((int64_t) (x)) * (y) / F)  // 두 고정 소수점 수를 곱함
#define MULFI(x, n) ((x) * (n))  // 고정 소수점을 정수와 곱함

#define DIV(x, y) (((int64_t) (x)) * F / (y))  // 두 고정 소수점 수를 나눔
#define DIVFI(x, n) ((x) / (n))  // 고정 소수점을 정수로 나눔



/* A kernel thread or user process.
 *
 * Each thread structure is stored in its own 4 kB page.  The
 * thread structure itself sits at the very bottom of the page
 * (at offset 0).  The rest of the page is reserved for the
 * thread's kernel stack, which grows downward from the top of
 * the page (at offset 4 kB).  Here's an illustration:
 *
 *      4 kB +---------------------------------+
 *           |          kernel stack           |
 *           |                |                |
 *           |                |                |
 *           |                V                |
 *           |         grows downward          |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           |                                 |
 *           +---------------------------------+
 *           |              magic              |
 *           |            intr_frame           |
 *           |                :                |
 *           |                :                |
 *           |               name              |
 *           |              status             |
 *      0 kB +---------------------------------+
 *
 * The upshot of this is twofold:
 *
 *    1. First, `struct thread' must not be allowed to grow too
 *       big.  If it does, then there will not be enough room for
 *       the kernel stack.  Our base `struct thread' is only a
 *       few bytes in size.  It probably should stay well under 1
 *       kB.
 *
 *    2. Second, kernel stacks must not be allowed to grow too
 *       large.  If a stack overflows, it will corrupt the thread
 *       state.  Thus, kernel functions should not allocate large
 *       structures or arrays as non-static local variables.  Use
 *       dynamic allocation with malloc() or palloc_get_page()
 *       instead.
 *
 * The first symptom of either of these problems will probably be
 * an assertion failure in thread_current(), which checks that
 * the `magic' member of the running thread's `struct thread' is
 * set to THREAD_MAGIC.  Stack overflow will normally change this
 * value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
 * the run queue (thread.c), or it can be an element in a
 * semaphore wait list (synch.c).  It can be used these two ways
 * only because they are mutually exclusive: only a thread in the
 * ready state is on the run queue, whereas only a thread in the
 * blocked state is on a semaphore wait list. */
struct dead_child{
	tid_t tid;
	int exit_code;
};

struct thread {
	/* Owned by thread.c. */
	tid_t tid;                          /* Thread identifier. */
	enum thread_status status;          /* Thread state. */
	char name[16];                      /* Name (for debugging purposes). */
	int priority;                       /* Priority. */
	int origin_priority;
	int donated_cnt;
	struct semaphore *sema;
	struct lock *waiting_lock; 
	
	int nice;
	int recent_cpu;
	/* Shared between thread.c and synch.c. */
	struct list_elem elem;              /* List element. */
	struct list_elem all_elem;          /* List element. */
	struct list_elem blocked_elem;

#ifdef USERPROG
	/* Owned by userprog/process.c. */
	uint64_t *pml4;                     /* Page map level 4 */
	void *fd_table[32];
	int exit_code;
	bool is_user;
	bool is_waited;
	struct thread *parent;
	struct semaphore *wait_sema;
	// tid_t children[64];
	struct list child_list;
	struct list_elem child_elem;
	int child_code;
	struct dead_child* dead_list[32];
	struct file* exec_file;
#endif
#ifdef VM
	/* Table for whole virtual memory owned by thread. */
	struct supplemental_page_table spt;
#endif

	/* Owned by thread.c. */
	struct intr_frame tf;               /* Information for switching */
	unsigned magic;                     /* Detects stack overflow. */
};

struct sleep_elem{
    struct list_elem elem;
    struct semaphore *sema;
    int64_t wake_t;
};

struct blocked_elem{
	struct list_elem elem;
	struct thread *thread;
};

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void do_iret (struct intr_frame *tf);

void thread_sleep (int64_t start, int64_t ticks);
void thread_wake (int64_t ticks);
void update_list(struct list* list, struct thread *t);
int nice_to_priority(struct thread *t, int nice);
void recalculate_priority(void);
void update_load_avg(void);
void thread_cpu (void);
void recalculate_recent_cpu(void);
struct thread*get_thread_by_tid (tid_t tid);

#endif /* threads/thread.h */

void Thread_Sleep(int64_t wakeTime);
void Thread_WakeUp();
void Thread_Preempt();

bool Donate_On_Set(int new_priority);


static bool sleep_less (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

static bool priority_less (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED);

void Set_Load_Avg();

void Fix_All_Recent_CPU();

void MLFQS_SetPriorities();

void Thread_Add_Recent_Cpu(struct thread* t);