#include <stdio.h>
#include <time.h>
#include "limits.h"
#include <pthread.h>

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant -lpthread 3.c -o a.out
 * $> ./a.out
 */

/**
 * Task description:
 *
 * ## (3) Pthread mutex lock
 * Points: 4.
 * pthread_mutex_lock/unlock in N threads. You need to start N
 * threads (customizable number), each is constantly doing mutex
 * lock/unlock. For each lock you need to increment a global atomic
 * counter. See how much time it takes to do certain amount of
 * locks/unlocks depending on thread count, PER ONE LOCK/UNLOCK pair.
 * Try at least
 * * 10 mln locks with 1 thread;
 * * 10 mln locks with 2 threads;
 * * 10 mln locks with 3 threads.
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
#define LOCKS_NUM 10000000LL

pthread_mutex_t mutex;
volatile int counter;  // global atomic counter

void* counter_inc(void *args) {
    (void)args;
    while (1) {
        pthread_mutex_lock(&mutex);
        if (counter == LOCKS_NUM) {
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        ++counter;  // increment a global atomic counter (int is an atomic
                    // according to https://www.gnu.org/software/libc/manual/html_node/Atomic-Types.html)
                    // if interpretation the phrase as "atomic increment of global counter" then the following
                    // should be used: "__sync_add_and_fetch(&counter, 1);")
        pthread_mutex_unlock(&mutex);
    }
}

void run_pthread_mutex_lock_benchmark(unsigned long long thread_num) {
    struct timespec start, end;
    unsigned long long max_dur_ns = 0;
    unsigned long long min_dur_ns = ULLONG_MAX;
    unsigned long long med_dur_ns = 0;
    for (unsigned long long run = 0; run < RUNS_NUM; ++run) {
        pthread_t threads[thread_num];
        counter = 0;
        pthread_mutex_init(&mutex, NULL);
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (unsigned long long i = 0; i < thread_num; ++i)
            pthread_create(&threads[i], NULL, counter_inc, NULL);
        for (unsigned long long i = 0; i < thread_num; ++i)
            pthread_join(threads[i], NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);
        pthread_mutex_destroy(&mutex);
        unsigned long long dur_ns = (((unsigned long long) (end.tv_sec - start.tv_sec)) * 1000000000LL
                                     + ((unsigned long long) (end.tv_nsec - start.tv_nsec)));
        if (dur_ns > max_dur_ns)
            max_dur_ns = dur_ns;
        if (dur_ns < min_dur_ns)
            min_dur_ns = dur_ns;
        med_dur_ns += dur_ns;
    }
    min_dur_ns /= LOCKS_NUM;
    max_dur_ns /= LOCKS_NUM;
    med_dur_ns /= RUNS_NUM * LOCKS_NUM;
    printf("(%lld threads) min duration: %lld ns per lock/unlock pair\n", thread_num, min_dur_ns);
    printf("(%lld threads) max duration: %lld ns per lock/unlock pair\n", thread_num, max_dur_ns);
    printf("(%lld threads) median duration: %lld ns per lock/unlock pair\n", thread_num, med_dur_ns);
}

int main() {
    printf("Benchmark statistics are calculated over %lld runs. Each run is %lld locks in total.\n",
           RUNS_NUM, LOCKS_NUM);
    run_pthread_mutex_lock_benchmark(1);
    run_pthread_mutex_lock_benchmark(2);
    run_pthread_mutex_lock_benchmark(3);
    run_pthread_mutex_lock_benchmark(4);
    run_pthread_mutex_lock_benchmark(5);
    return 0;
}
