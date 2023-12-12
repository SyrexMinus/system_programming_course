#include <stdio.h>
#include <time.h>
#include "limits.h"
#include <pthread.h>

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant -lpthread 6.c -o a.out
 * $> ./a.out
 */

/**
 * Task description:
 *
 * ## (6) Pthread cond signaling
 * Points: 4.
 * Need to measure cost of signal vs broadcast depending on thread
 * count. You need to start 1 thread doing the signaling and N
 * threads doing pthread_cond_wait(). The condvar needs to be
 * protected with a mutex. You need to measure how much time it takes
 * to make certain number of signaling calls. The signaling thread
 * need to either call pthread_cond_signal() or
 * pthread_cond_broadcast(). Depending on bench params. Try at least
 * these combinations:
 * * 1 mln signal()s with 1 wait-thread;
 * * 1 mln broadcast()s with 1 wait-thread;
 * * 1 mln signal()s with 3 wait-threads;
 * * 1 mln broadcast()s with 3 wait-threads.
 *
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
#define SIG_NUM 1000000LL

pthread_mutex_t mutex;
pthread_cond_t cond;
volatile int is_complete;

void* signaling(void *args) {
    int is_bc_mode = *((int*)args);
    if (is_bc_mode) {
        for (int i = 0; i < SIG_NUM; ++i) {
            pthread_mutex_lock(&mutex);
            pthread_cond_broadcast(&cond);
            pthread_mutex_unlock(&mutex);
        }
    } else {
        for (int i = 0; i < SIG_NUM; ++i) {
            pthread_mutex_lock(&mutex);
            pthread_cond_signal(&cond);
            pthread_mutex_unlock(&mutex);
        }
    }
    pthread_mutex_lock(&mutex);
    is_complete = 1;
    pthread_cond_broadcast(&cond);  // broadcast as all the consumers should exit
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void* consuming(void *args) {
    (void)args;
    pthread_mutex_lock(&mutex);
    while (!is_complete)
        pthread_cond_wait(&cond, &mutex);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void run_pthread_cond_signaling_benchmark(unsigned long long thread_num, int is_bc_mode, const char *mode_name) {
    struct timespec start, end;
    unsigned long long max_dur_ns = 0;
    unsigned long long min_dur_ns = ULLONG_MAX;
    unsigned long long med_dur_ns = 0;
    for (unsigned long long run = 0; run < RUNS_NUM; ++run) {
        pthread_t sig_thread;
        pthread_t wait_threads[thread_num];
        pthread_mutex_init(&mutex, NULL);
        pthread_cond_init(&cond, NULL);
        is_complete = 0;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (unsigned long long i = 0; i < thread_num; ++i)
            pthread_create(&wait_threads[i], NULL, consuming, NULL);
        pthread_create(&sig_thread, NULL, signaling, &is_bc_mode);
        for (unsigned long long i = 0; i < thread_num; ++i)
            pthread_join(wait_threads[i], NULL);
        pthread_join(sig_thread, NULL);
        clock_gettime(CLOCK_MONOTONIC, &end);
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mutex);
        unsigned long long dur_ns = (((unsigned long long) (end.tv_sec - start.tv_sec)) * 1000000000LL
                                     + ((unsigned long long) (end.tv_nsec - start.tv_nsec)));
        if (dur_ns > max_dur_ns)
            max_dur_ns = dur_ns;
        if (dur_ns < min_dur_ns)
            min_dur_ns = dur_ns;
        med_dur_ns += dur_ns;
    }
    double max_dur_s = (double) max_dur_ns / 1000000000;
    double min_dur_s = (double) min_dur_ns / 1000000000;
    double med_dur_s = (double) med_dur_ns / RUNS_NUM / 1000000000;
    printf("(%s()s with %lld wait-threads) min duration: %lf sec\n", mode_name, thread_num, min_dur_s);
    printf("(%s()s with %lld wait-threads) max duration: %lf sec\n", mode_name, thread_num, max_dur_s);
    printf("(%s()s with %lld wait-threads) median duration: %lf sec\n", mode_name, thread_num, med_dur_s);
}

int main() {
    printf("Benchmark statistics are calculated over %lld runs. Each run is %lld signals in total.\n",
           RUNS_NUM, SIG_NUM);
    run_pthread_cond_signaling_benchmark(1, 0, "signal");
    run_pthread_cond_signaling_benchmark(1, 1, "broadcast");
    run_pthread_cond_signaling_benchmark(3, 0, "signal");
    run_pthread_cond_signaling_benchmark(3, 1, "broadcast");
    return 0;
}
