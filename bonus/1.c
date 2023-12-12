#include <stdio.h>
#include <time.h>
#include "limits.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant 1.c -o a.out
 * $> ./a.out
 */

/**
 * Task description:
 *
 * ## (1) Clock bench
 * Points: 3.
 * Bench clock_gettime() with CLOCK_REALTIME, CLOCK_MONOTONIC,
 * CLOCK_MONOTONIC_RAW. Run the function with those params 50 mln
 * times and see how much time it took PER ONE CALL.
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
#define CALLS_NUM 50000000LL

void run_clock_gettime_benchmark(clockid_t clock_id, const char *clock_name) {
    struct timespec start, end, tmp;
    unsigned long long max_dur_ns = 0;
    unsigned long long min_dur_ns = ULLONG_MAX;
    unsigned long long med_dur_ns = 0;
    for (unsigned long long run = 0; run < RUNS_NUM; ++run) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for (unsigned long long i = 0; i < CALLS_NUM; ++i)
            clock_gettime(clock_id, &tmp);
        clock_gettime(CLOCK_MONOTONIC, &end);
        unsigned long long dur_ns = (((unsigned long long) (end.tv_sec - start.tv_sec)) * 1000000000LL
                                     + ((unsigned long long) (end.tv_nsec - start.tv_nsec)));
        if (dur_ns > max_dur_ns)
            max_dur_ns = dur_ns;
        if (dur_ns < min_dur_ns)
            min_dur_ns = dur_ns;
        med_dur_ns += dur_ns;
    }
    min_dur_ns /= CALLS_NUM;
    max_dur_ns /= CALLS_NUM;
    med_dur_ns /= RUNS_NUM * CALLS_NUM;
    printf("%s min duration: %lld ns per call\n", clock_name, min_dur_ns);
    printf("%s max duration: %lld ns per call\n", clock_name, max_dur_ns);
    printf("%s median duration: %lld ns per call\n", clock_name, med_dur_ns);
}

int main() {
    printf("Benchmark statistics are calculated over %lld runs. Each run is %lld calls.\n", RUNS_NUM, CALLS_NUM);
    run_clock_gettime_benchmark(CLOCK_REALTIME, "CLOCK_REALTIME");
    run_clock_gettime_benchmark(CLOCK_MONOTONIC, "CLOCK_MONOTONIC");
    run_clock_gettime_benchmark(CLOCK_MONOTONIC_RAW, "CLOCK_MONOTONIC_RAW");
    return 0;
}
