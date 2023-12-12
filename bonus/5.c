#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "limits.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant -lpthread 5.c -o a.out
 * $> ./a.out
 */

/**
 * Task description:
 *
 * ## (5) Atomic increment
 * Points: 3.
 * Atomic increment in N threads. You need to start N threads
 * (customizable number), each is constantly doing atomic lock-free
 * increment on a certain variable. See how much time it takes to
 * reach certain value on that variable depending on thread count and
 * memory order. Try at least
 * * 100 mln increments with 1 thread and relaxed order;
 * * 100 mln increments with 3 threads and relaxed order;
 * * 100 mln increments with 3 threads and sequentially consistent
 *   order.
 *
 * Each benchmark is basically calling some function or functions in
 * a loop many thousands or even millions of times. You have to
 * save a timestamp before and after the loop, find the duration, and
 * print it. To reduce the amount of noise you need to run the
 * benchmark multiple times and use min, max and median durations as
 * the result.
 *
 * For bench parameters you can either try to do it via the command
 * line arguments or just as const global variables in the code.
 *
 * For time measurement you have to use
 * clock_gettime(CLOCK_MONOTONIC).
 */

#define RUNS_NUM 10LL
#define INC_NUM 100000000LL

// reference https://stackoverflow.com/questions/1961173/median-function-in-c-math-library
int
compare(const void *a, const void *b)
{
    long long res = *(unsigned long long *)a - *(unsigned long long *)b;
    if (res < 0)
        return -1;
    else if (res > 0)
        return 1;
    return 0;
}

volatile int counter;

void *
counter_inc(void *args)
{
    int order = *((int *)args);
    while (1) {
        if (counter >= INC_NUM) {
            return NULL;
        }
        __atomic_add_fetch(&counter, 1, order);
    }
}

void
run_atomic_increment_benchmark(unsigned long long thread_num, int order, const char *order_name)
{
    struct timespec start, end;
    unsigned long long max_dur_ns = 0;
    unsigned long long min_dur_ns = ULLONG_MAX;
    unsigned long long dur_nss[RUNS_NUM];
    for (unsigned long long run = 0; run < RUNS_NUM; ++run) {
        pthread_t threads[thread_num];
        counter = 0;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (unsigned long long i = 0; i < thread_num; ++i)
            pthread_create(&threads[i], NULL, counter_inc, &order);
        for (unsigned long long i = 0; i < thread_num; ++i)
            pthread_join(threads[i], NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);
        unsigned long long dur_ns = (((unsigned long long) (end.tv_sec - start.tv_sec)) * 1000000000LL
                                     + ((unsigned long long) (end.tv_nsec - start.tv_nsec)));
        if (dur_ns > max_dur_ns)
            max_dur_ns = dur_ns;
        if (dur_ns < min_dur_ns)
            min_dur_ns = dur_ns;
        dur_nss[run] = dur_ns;
    }
    double max_dur_s = (double) max_dur_ns / 1000000000;
    double min_dur_s = (double) min_dur_ns / 1000000000;
    qsort(dur_nss, RUNS_NUM, sizeof(unsigned long long), compare);
    double med_dur_s = (double) dur_nss[RUNS_NUM/2] / 1000000000;
    printf("(%lld threads with %s order) min duration: %lf sec\n",
           thread_num, order_name, min_dur_s);
    printf("(%lld threads with %s order) max duration: %lf sec\n",
           thread_num, order_name, max_dur_s);
    printf("(%lld threads with %s order) median duration: %lf sec\n",
           thread_num, order_name, med_dur_s);
}

int
main()
{
    printf("Benchmark statistics are calculated over %lld runs. Each run is %lld increments in total.\n",
           RUNS_NUM, INC_NUM);
    run_atomic_increment_benchmark(1, __ATOMIC_RELAXED, "relaxed");
    run_atomic_increment_benchmark(3, __ATOMIC_RELAXED, "relaxed");
    run_atomic_increment_benchmark(3, __ATOMIC_SEQ_CST, "sequentially consistent");
    return 0;
}
