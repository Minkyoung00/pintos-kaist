/* Creates N threads, each of which sleeps a different, fixed
   duration, M times.  Records the wake-up order and verifies
   that it is valid. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static void test_sleep (int thread_cnt, int iterations);

void
test_alarm_single (void) 
{
  test_sleep (5, 1);
}

void
test_alarm_multiple (void) 
{
  test_sleep (5, 7);
}

/* Information about the test. */
struct sleep_test 
  {
    int64_t start;              /* Current time at start of test. */
    int iterations;             /* Number of iterations per thread. */

    /* Output. */
    struct lock output_lock;    /* Lock protecting output buffer. */
    int *output_pos;            /* Current position in output buffer. */
  };

/* Information about an individual thread in the test. */
struct sleep_thread 
  {
    struct sleep_test *test;     /* Info shared between all threads. */
    int id;                     /* Sleeper ID. */
    int duration;               /* Number of ticks to sleep. */
    int iterations;             /* Iterations counted so far. */
  };

static void sleeper (void *);

/* Runs THREAD_CNT threads thread sleep ITERATIONS times each. */
static void
test_sleep (int thread_cnt, int iterations) 
{
  struct sleep_test test;
  struct sleep_thread *threads;
  int *output, *op;
  int product;
  int i;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  msg ("Creating %d threads to sleep %d times each.", thread_cnt, iterations);
  msg ("Thread 0 sleeps 10 ticks each time,");
  msg ("thread 1 sleeps 20 ticks each time, and so on.");
  msg ("If successful, product of iteration count and");
  msg ("sleep duration will appear in nondescending order.");

  /* Allocate memory. */
  threads = malloc (sizeof *threads * thread_cnt);
  output = malloc (sizeof *output * iterations * thread_cnt * 2);

  if (threads == NULL || output == NULL)
    PANIC ("couldn't allocate memory for test");

  /* Initialize test. */
  test.start = timer_ticks () + 100; // 현재로부터 100 tick 후로 start 시간 설정
  test.iterations = iterations;
  lock_init (&test.output_lock);
  test.output_pos = output;

  /* Start threads. */
  ASSERT (output != NULL);
  for (i = 0; i < thread_cnt; i++)
    {
      struct sleep_thread *t = threads + i;
      char name[16];
      
      t->test = &test;
      t->id = i;
      t->duration = (i + 1) * 10; // 재우는 시간 설정
      t->iterations = 0;

      snprintf (name, sizeof name, "thread %d", i);
      thread_create (name, PRI_DEFAULT, sleeper, t); // sleep 함수 내 작업을 수행하는 스래드 생성
    }
  
  /* Wait long enough for all the threads to finish. */
  timer_sleep (100 + thread_cnt * iterations * 10 + 100); // 모든 스레드들의 작업이 끝날 때 동안 기다려주기(timer_sleep)
  // msg("End of waiting for the end of peer threads");


  /* Acquire the output lock in case some rogue thread is still
     running. */
  lock_acquire (&test.output_lock); // 다른 스레드(alarm-wait 외)가 실행 중일 경우, lock 권한을 가져옴

  /* Print completion order. */
  product = 0;
  for (op = output; op < test.output_pos; op++) 
    {
      struct sleep_thread *t;
      int new_prod;

      ASSERT (*op >= 0 && *op < thread_cnt);
      t = threads + *op;

      new_prod = ++t->iterations * t->duration;
        
      msg ("thread %d: duration=%d, iteration=%d, product=%d",
           t->id, t->duration, t->iterations, new_prod);
      
      if (new_prod >= product)
        product = new_prod;
      else
        fail ("thread %d woke up out of order (%d > %d)!",
              t->id, product, new_prod);
    }

  /* Verify that we had the proper number of wakeups. */
  for (i = 0; i < thread_cnt; i++)
    if (threads[i].iterations != iterations)
      fail ("thread %d woke up %d times instead of %d",
            i, threads[i].iterations, iterations);
  
  lock_release (&test.output_lock); // lock 반납
  free (output);
  free (threads);
}

/* Sleeper thread. */
static void
sleeper (void *t_) // t->test = &test; test 정보
                   // t->id = i; 
                   // t->duration = (i + 1) * 10; // 재우는 시간 설정
                   // t->iterations = 0;
{
  struct sleep_thread *t = t_;
  struct sleep_test *test = t->test; // start 시간, iteration 횟수, output 위치가 담겨 있음
  int i;

  for (i = 1; i <= test->iterations; i++) 
    {
      int64_t sleep_until = test->start + i * t->duration;  // start 시간에 재울 시간(iteration 만큼 곱한)을 더한 시간을 깨우는 시간(sleep_until)으로 설정
      timer_sleep (sleep_until - timer_ticks ());           // 재울 시간(깨울 시간 - 이미 지난 시간) 만큼 재우기
      lock_acquire (&test->output_lock);                    // 결과를 입력하는 중 방해 받지 않기 위해 lock 얻기
      *test->output_pos++ = t->id;                          // 결과 입력
      lock_release (&test->output_lock);                    // lock 권한 반납
    }
}
