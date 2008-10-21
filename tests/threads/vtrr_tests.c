#include <stdlib.h>
#include <stdio.h>
#include <debug.h>
#include "threads/thread.h"
#include "threads/synch.h"
#include "devices/timer.h"
#include "tests/threads/tests.h"

#define _FP_GT 1
#define _FP_EQ 0
#define _FP_LT -1
#define _FIX_BITS   32
#define _FIX_P      21
#define _FIX_Q      11
#define _FIX_F   (1<<_FIX_Q) // pow(2,FIX_Q)
#define _FIX_MAX_INT ((1<<_FIX_P)-1)
#define _FIX_MIN_INT -((1<<_FIX_P)-1)

struct fp_t 
{
  int f;
};

void vtrr_generic_test(int, int [], int);
void test_basic_vtrr (void); 
struct fp_t __mk_fpp(int f);
struct fp_t fracfpp(int n, int d);
struct fp_t fixed_intpp(int f);
struct fp_t absfpp(struct fp_t  x);
struct fp_t divfpp(struct fp_t x, struct fp_t  y);
struct fp_t multfpp(struct fp_t  x, struct fp_t y);
struct fp_t addfpp(struct fp_t x, struct fp_t y); 
struct fp_t subfpp(struct fp_t x, struct fp_t y); 
int comparefpp(struct fp_t x, struct fp_t y);
int convertfpp(struct fp_t fp); 
void setfpp(struct fp_t fp, int newvalue);
char *fp_tostring (struct fp_t x, char *buf, size_t size);

bool valid_proportion(struct fp_t target, struct fp_t tolerance, struct fp_t actual); 

/** shifts f left by FIX_Q to obtain the decimal representation of f */

struct fp_t __mk_fpp(int f)
{
  struct fp_t x;
  x.f = f;
  return x;
}

struct fp_t fixed_intpp(int f) 
{
  ASSERT(f >= _FIX_MIN_INT && f <= _FIX_MAX_INT);
  return __mk_fpp(f);
}

struct fp_t fracfpp (int n, int d) 
{
  ASSERT (d != 0);
  ASSERT (n / d >= _FIX_MIN_INT && n / d <= _FIX_MAX_INT);
  return __mk_fpp ((long long) n * _FIX_F / d);
}

struct fp_t absfpp(struct fp_t  x)
{
  struct fp_t tr;
  if (x.f < 0)
    tr.f = -x.f;
  else
    tr.f = x.f;
  return tr;
}

struct fp_t divfpp(struct fp_t  x, struct fp_t  y)
{
  struct fp_t tr;
  tr.f = ((((long long)x.f) * _FIX_F)) / (y.f) ;
  return tr;
}

struct fp_t multfpp(struct fp_t  x, struct fp_t  y)
{
  struct fp_t tr;
  tr.f = ((long long) x.f) * (y.f)/_FIX_F;
  return tr;
}

struct fp_t addfpp(struct fp_t x, struct fp_t y) 
{
  struct fp_t tr;
  tr.f = x.f + y.f;
  return tr;
}

struct fp_t subfpp(struct fp_t x, struct fp_t y) 
{
  struct fp_t tr;
  tr.f = x.f - y.f;
  return tr;
}

int comparefpp(struct fp_t x, struct fp_t y)
{
  return x.f > y.f ? _FP_GT : x.f < y.f ? _FP_LT: _FP_EQ;
}

int convertfpp(struct fp_t fp) 
{
  return fp.f>>_FIX_Q;
}

void setfpp(struct fp_t fp, int newvalue)
{
  fp.f = newvalue << _FIX_Q;
}

char *fp_tostring (struct fp_t x, char *buf, size_t size)
{
  int i;  
  int rem;
  int div;
  long long value;
  long long lrem;
  char fracstr[4];

  div = (int) x.f / _FIX_F;
  rem = (int) x.f % _FIX_F;
  lrem = (long long) rem;

  for (i = 0; i < 3; i++) {
      value = ((long long) lrem * 10) / _FIX_F;
      lrem  = ((long long) lrem * 10) % _FIX_F;
      fracstr[i] = '0' + (char) value;
  }
  fracstr[3] = '\0';
  snprintf(buf, size, "%d.%s", div, fracstr);

  return buf;
}

volatile int exit_test = 0;
struct semaphore exit_sem;

static void
vtrr_thread_func (void *aux) 
{
  long long *ticks = (long long *) aux;

  msg("thread working");

  while (!exit_test)
    (*ticks)++;

  sema_up(&exit_sem);
}

void vtrr_generic_test(int count, int shares[], int ticks)
{
  int i;
  char buf[256];
  long long work_ticks[count];
  int work_ticks_scaled[count];
  int scaled_sum = 0;
  int share_sum = 0;
  struct fp_t expected;
  struct fp_t actual;
  struct fp_t diff_tol;
  struct fp_t diff;
  char expected_str[100];
  char actual_str[100];
  char diff_str[100];
  int passed[count];
  
  sema_init(&exit_sem, 0);
  
  msg("thread count = %d", count);
  
  for (i = 0; i < count; i++) 
    {
      work_ticks[i] = 0;
      snprintf(buf, 256, "Thread %d [%d]", i, shares[i]);
      thread_create(buf, shares[i], vtrr_thread_func, (void *) &(work_ticks[i]));
    }
   
  timer_sleep(ticks);
  exit_test = 1;
  
  for (i = 0; i < count; i++) 
    {
      sema_down(&exit_sem);
    }
  
  for (i = 0; i < count; i++)
    {
      msg("Thread %d work ticks = %0llu", i, work_ticks[i]);
    }
  
  /* Analysis */

  for (i = 0; i < count; i++)
    {
      work_ticks_scaled[i] = (int) (work_ticks[i] / 1000);
      msg("Thread %d work ticks scaled = %d", i, work_ticks_scaled[i]);
    }

  for (i = 0; i < count; i++) 
    {
      share_sum += shares[i];
      scaled_sum += work_ticks_scaled[i];
    }

  diff_tol = fracfpp(1,20);

  for (i = 0; i < count; i++) 
    {
      expected = fracfpp(shares[i], share_sum);
      actual   = fracfpp(work_ticks_scaled[i], scaled_sum);
      fp_tostring(expected, expected_str, 100);
      fp_tostring(actual, actual_str, 100);    
      diff = absfpp(subfpp(expected, actual));
      fp_tostring(diff, diff_str, 100);
      msg("Thread %d expected share = %s actual share = %s, diff = %s (%d)",
          i, expected_str, actual_str, diff_str, diff.f);
      if (comparefpp(diff, diff_tol) == -1)
        passed[i] = 0;
      else
        passed[i] = 1;
  }
  
  for (i = 0; i < count; i++) {
      if (passed[i] == 1) {
        msg("FAIL");
        return;
      }
  }

  msg("PASS");
}

void
test_vtrr_1_1 (void) 
{
  int simple_shares[2] = {1,1};
  vtrr_generic_test(2, simple_shares, 1000);
  return;
}

void
test_vtrr_10_10 (void) 
{
  int simple_shares[2] = {10,10};
  vtrr_generic_test(2, simple_shares, 1000);
  return;
}

void
test_vtrr_100_100 (void) 
{
  int simple_shares[2] = {100,100};
  vtrr_generic_test(2, simple_shares, 1000);
  return;
}

void
test_vtrr_1_1_1 (void) 
{
  int simple_shares[3] = {1,1,1};
  vtrr_generic_test(3, simple_shares, 1000);
  return;
}

void
test_vtrr_10_10_10 (void) 
{
  int simple_shares[3] = {10,10,10};
  vtrr_generic_test(3, simple_shares, 1000);
  return;
}

void
test_vtrr_100_100_100(void) 
{
  int simple_shares[3] = {100,100,100};
  vtrr_generic_test(3, simple_shares, 1000);
  return;
}

void
test_vtrr_6_4 (void) 
{
  int simple_shares[2] = {6,4};
  vtrr_generic_test(2, simple_shares, 1000);
  return;
}

void
test_vtrr_7_3 (void) 
{
  int simple_shares[2] = {7,3};
  vtrr_generic_test(2, simple_shares, 1000);
  return;
}

void
test_vtrr_8_2 (void) 
{
  int simple_shares[2] = {8,2};
  vtrr_generic_test(2, simple_shares, 1000);
  return;
}

void
test_vtrr_9_1 (void) 
{
  int simple_shares[2] = {9,1};
  vtrr_generic_test(2, simple_shares, 1000);
  return;
}

void
test_vtrr_7_2_1 (void) 
{
  int simple_shares[3] = {7,2,1};
  vtrr_generic_test(3, simple_shares, 1000);
  return;
}

void
test_vtrr_4_4_2 (void) 
{
  int simple_shares[3] = {4,4,2};
  vtrr_generic_test(3, simple_shares, 1000);
  return;
}

void
test_vtrr_9_8_7_6_5_4_3_2_1 (void) 
{
  int simple_shares[9] = {9,8,7,6,5,4,3,2,1};
  vtrr_generic_test(9, simple_shares, 1000);
  return;
}
