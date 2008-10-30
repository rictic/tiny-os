#ifndef TESTS_THREADS_TESTS_H
#define TESTS_THREADS_TESTS_H

void run_test (const char *);

typedef void test_func (void);

extern test_func test_vtrr_1_1;
extern test_func test_vtrr_10_10;
extern test_func test_vtrr_100_100;
extern test_func test_vtrr_1_1_1;
extern test_func test_vtrr_10_10_10;
extern test_func test_vtrr_100_100_100;
extern test_func test_vtrr_6_4;
extern test_func test_vtrr_7_3;
extern test_func test_vtrr_8_2;
extern test_func test_vtrr_9_1;
extern test_func test_vtrr_7_2_1;
extern test_func test_vtrr_4_4_2;
extern test_func test_vtrr_9_8_7_6_5_4_3_2_1;

void msg (const char *, ...);
void fail (const char *, ...);
void pass (void);

#endif /* tests/threads/tests.h */

