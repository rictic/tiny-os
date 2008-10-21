#include "tests/threads/tests.h"
#include <debug.h>
#include <string.h>
#include <stdio.h>

struct test 
  {
    const char *name;
    test_func *function;
  };

static const struct test tests[] = 
  {
    {"vtrr_1_1", test_vtrr_1_1},
    {"vtrr_10_10", test_vtrr_10_10},
    {"vtrr_100_100", test_vtrr_100_100},
    {"vtrr_1_1_1", test_vtrr_1_1_1},
    {"vtrr_10_10_10", test_vtrr_10_10_10},
    {"vtrr_100_100_100", test_vtrr_100_100_100},
    {"vtrr_6_4", test_vtrr_6_4},
    {"vtrr_7_3", test_vtrr_7_3},
    {"vtrr_8_2", test_vtrr_8_2},
    {"vtrr_9_1", test_vtrr_9_1},
    {"vtrr_7_2_1", test_vtrr_7_2_1},
    {"vtrr_4_4_2", test_vtrr_4_4_2},
    {"vtrr_9_8_7_6_5_4_3_2_1", test_vtrr_9_8_7_6_5_4_3_2_1},
  };

static const char *test_name;

/* Runs the test named NAME. */
void
run_test (const char *name) 
{
  const struct test *t;

  for (t = tests; t < tests + sizeof tests / sizeof *tests; t++)
    if (!strcmp (name, t->name))
      {
        test_name = name;
        msg ("begin");
        t->function ();
        msg ("end");
        return;
      }
  PANIC ("no test named \"%s\"", name);
}

/* Prints FORMAT as if with printf(),
   prefixing the output by the name of the test
   and following it with a new-line character. */
void
msg (const char *format, ...) 
{
  va_list args;
  
  printf ("(%s) ", test_name);
  va_start (args, format);
  vprintf (format, args);
  va_end (args);
  putchar ('\n');
}

/* Prints failure message FORMAT as if with printf(),
   prefixing the output by the name of the test and FAIL:
   and following it with a new-line character,
   and then panics the kernel. */
void
fail (const char *format, ...) 
{
  va_list args;
  
  printf ("(%s) FAIL: ", test_name);
  va_start (args, format);
  vprintf (format, args);
  va_end (args);
  putchar ('\n');

  PANIC ("test failed");
}

/* Prints a message indicating the current test passed. */
void
pass (void) 
{
  printf ("(%s) PASS\n", test_name);
}

