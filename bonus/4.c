#include <stdio.h>
#include <time.h>
#include "limits.h"
#include <pthread.h>

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant -lpthread 4.c -o a.out
 * $> ./a.out
 */

/**
 * Task description:
 *
 * ## (4) Pthread create + join
 * Points: 2.
 * You need to create and join a dummy thread with an empty function
 * some number of times. To see how much time one create+join costs
 * approximately, PER ONE PAIR. Do it in a loop 100k times, measure
 * the time per one create+join.
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
#define CJ_PAIR_NUM 100000LL

void* dummy(void *args) {
    (void)args;
    return NULL;
}

void run_pthread_create_join_benchmark() {
    struct timespec start, end;
    unsigned long long max_dur_ns = 0;
    unsigned long long min_dur_ns = ULLONG_MAX;
    unsigned long long med_dur_ns = 0;
    for (unsigned long long run = 0; run < RUNS_NUM; ++run) {
        pthread_t thread;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (unsigned long long i = 0; i < CJ_PAIR_NUM; ++i) {
            pthread_create(&thread, NULL, dummy, NULL);
            pthread_join(thread, NULL);
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        unsigned long long dur_ns = (((unsigned long long) (end.tv_sec - start.tv_sec)) * 1000000000LL
                                     + ((unsigned long long) (end.tv_nsec - start.tv_nsec)));
        if (dur_ns > max_dur_ns)
            max_dur_ns = dur_ns;
        if (dur_ns < min_dur_ns)
            min_dur_ns = dur_ns;
        med_dur_ns += dur_ns;
    }
    min_dur_ns /= CJ_PAIR_NUM;
    max_dur_ns /= CJ_PAIR_NUM;
    med_dur_ns /= RUNS_NUM * CJ_PAIR_NUM;
    printf("min duration: %lld ns per create+join\n", min_dur_ns);
    printf("max duration: %lld ns per create+join\n", max_dur_ns);
    printf("median duration: %lld ns per create+join\n", med_dur_ns);
}

int main() {
    printf("Benchmark statistics are calculated over %lld runs. Each run is %lld create+join.\n",
           RUNS_NUM, CJ_PAIR_NUM);
    run_pthread_create_join_benchmark();
    return 0;
}
