#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include "limits.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant -lpthread 7.c -o a.out
 * $> ./a.out
 */

/**
 * Task description:
 *
 * ## (7) False sharing
 * Points: 4.
 * You need to check how strongly threads can affect each other even
 * if they don't access the same data. You need to create an array of
 * uint64_t numbers. And then start N threads. Each thread should
 * increment its own number in the array in a loop.
 *
 * Make sure the compiler won't turn the loop into a single +=
 * operation. For that you can try to add a check on a dummy volatile
 * variable in the loop's condition.
 *
 * Assuming the threads are in an array, thread with index A should
 * use number arr[A]. See how much time it takes for all the threads
 * to reach certain value of their numbers.
 *
 * Then make the threads use numbers on a distance from each other.
 * So thread with index A uses number arr[A * 8]. Distance between
 * numbers should be at least 64 bytes. Repeat the bench. You need to
 * try at least the following combinations:
 * * 100 mln increments with 1 thread;
 * * 100 mln increments with 2 threads with close numbers;
 * * 100 mln increments with 2 threads with distant numbers;
 * * 100 mln increments with 3 threads with close numbers;
 * * 100 mln increments with 3 threads with distant numbers.
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

struct thread_args {
    unsigned long long i;
    uint64_t *arr;
    unsigned long long inc_num;
};

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

void *
counter_inc(void *args)
{
    struct thread_args args_ = *((struct thread_args *)args);
    unsigned long long i = args_.i;
    uint64_t *arr = args_.arr;
    unsigned long long inc_num = args_.inc_num;
    volatile int v_true = 1;
    for (; arr[i] < inc_num && v_true; ++arr[i]);
    return NULL;
}

void
run_false_sharing_benchmark(unsigned long long thread_num, int dist, const char *dist_name)
{
    struct timespec start, end;
    unsigned long long max_dur_ns = 0;
    unsigned long long min_dur_ns = ULLONG_MAX;
    unsigned long long dur_nss[RUNS_NUM];
    for (unsigned long long run = 0; run < RUNS_NUM; ++run) {
        uint64_t arr[thread_num * dist];
        for (unsigned long long i = 0; i < thread_num * dist; i++)
            arr[i] = 0;
        pthread_t threads[thread_num];
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (unsigned long long i = 0; i < thread_num; ++i) {
            struct thread_args args;
            args.i = i * dist;
            args.arr = arr;
            args.inc_num = INC_NUM / thread_num;
            pthread_create(&threads[i], NULL, counter_inc, &args);
        }
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
    printf("(%lld threads with %s numbers) min duration: %lf sec\n",
           thread_num, dist_name, min_dur_s);
    printf("(%lld threads with %s numbers) max duration: %lf sec\n",
           thread_num, dist_name, max_dur_s);
    printf("(%lld threads with %s numbers) median duration: %lf sec\n",
           thread_num, dist_name, med_dur_s);
}

int
main()
{
    printf("Benchmark statistics are calculated over %lld runs. Each run is %lld increments in total.\n",
           RUNS_NUM, INC_NUM);
    run_false_sharing_benchmark(1, 1, "close");
    run_false_sharing_benchmark(2, 1, "close");
    run_false_sharing_benchmark(2, 8, "distant");
    run_false_sharing_benchmark(3, 1, "close");
    run_false_sharing_benchmark(3, 8, "distant");
    return 0;
}
