#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "libcoro.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> python3 generator.py -f test1.txt -c 10000 -m 10000
 * $> python3 generator.py -f test2.txt -c 10000 -m 10000
 * $> python3 generator.py -f test3.txt -c 10000 -m 10000
 * $> python3 generator.py -f test4.txt -c 10000 -m 10000
 * $> python3 generator.py -f test5.txt -c 10000 -m 10000
 * $> python3 generator.py -f test6.txt -c 100000 -m 10000
 * $> gcc -Wextra -Werror -Wall -Wno-gnu-folding-constant solution.c libcoro.c -o main
 * $> ./main 1000 3 test1.txt test2.txt test3.txt test4.txt test5.txt test6.txt
 */

struct my_context {
    char ** filenames;
    int ** storages;
    int * storage_sizes;
    double * work_time_msec;
    struct timespec time_counter_start;
    long coro_target_latency_us;
    int arrays_num;
    int * file_statuses;
};

static struct my_context *
my_context_new(char ** const filenames, int ** const storages, int * const storage_sizes, double * const work_time_msec,
               const long coro_target_latency_us, const int arrays_num, int * const file_statuses)
{
    struct my_context *ctx = malloc(sizeof(*ctx));
    ctx->filenames = filenames;
    ctx->storages = storages;
    ctx->storage_sizes = storage_sizes;
    ctx->work_time_msec = work_time_msec;
    *ctx->work_time_msec = 0;
    struct timespec clock;
    clock_gettime(CLOCK_MONOTONIC, &clock);
    ctx->time_counter_start = clock;
    ctx->coro_target_latency_us = coro_target_latency_us;
    ctx->arrays_num = arrays_num;
    ctx->file_statuses = file_statuses;
    return ctx;
}

static double
get_counted_time_msec(struct my_context *ctx)
{
    struct timespec clock;
    clock_gettime(CLOCK_MONOTONIC, &clock);
    return (clock.tv_sec-ctx->time_counter_start.tv_sec)*1.0e3 + (clock.tv_nsec-ctx->time_counter_start.tv_nsec)/1.0e6;
}

static void
add_work_time_msec(struct my_context *ctx)
{
    *ctx->work_time_msec += get_counted_time_msec(ctx);
}

static void
start_time_counter(struct my_context *ctx)
{
    struct timespec clock;
    clock_gettime(CLOCK_MONOTONIC, &clock);
    ctx->time_counter_start = clock;
}

static void
my_context_delete(struct my_context *ctx)
{
    add_work_time_msec(ctx);
    free(ctx);
}

static void
timely_coro_yield(struct my_context *ctx)
{
    double time_passed_us = get_counted_time_msec(ctx) * 1.0e3;
    if (time_passed_us > ctx->coro_target_latency_us) {
        add_work_time_msec(ctx);
        coro_yield();
        start_time_counter(ctx);
    }
}

/**
 * Merge sort algorithm. Time complexity - O(n*log(n))
 * reference: https://www.geeksforgeeks.org/merge-sort/
 * */
static void
_merge(int numbers[], const int left_bnd, const int middle, const int right_bnd) {
    const int left_arr_size = middle - left_bnd + 1;
    const int right_arr_size = right_bnd - middle;

    int left_arr[left_arr_size], right_arr[right_arr_size];
    for (int i = 0; i < left_arr_size; i++) {
        left_arr[i] = numbers[left_bnd + i];
    }
    for (int i = 0; i < right_arr_size; i++) {
        right_arr[i] = numbers[middle + 1 + i];
    }

    int left_arr_i = 0, right_arr_i = 0;
    int replace_i = left_bnd;
    while (left_arr_i < left_arr_size && right_arr_i < right_arr_size) {
        if (left_arr[left_arr_i] <= right_arr[right_arr_i]) {
            numbers[replace_i] = left_arr[left_arr_i];
            left_arr_i++;
        }
        else {
            numbers[replace_i] = right_arr[right_arr_i];
            right_arr_i++;
        }
        replace_i++;
    }

    while (left_arr_i < left_arr_size) {
        numbers[replace_i] = left_arr[left_arr_i];
        left_arr_i++;
        replace_i++;
    }
    while (right_arr_i < right_arr_size) {
        numbers[replace_i] = right_arr[right_arr_i];
        right_arr_i++;
        replace_i++;
    }
}

static void
_merge_sort(int numbers[], int const left_bnd, const int right_bnd, struct my_context * const ctx)
{
    if (left_bnd < right_bnd) {
        const int middle = left_bnd + (right_bnd - left_bnd) / 2;
        timely_coro_yield(ctx);
        _merge_sort(numbers, left_bnd, middle, ctx);
        timely_coro_yield(ctx);
        _merge_sort(numbers, middle + 1, right_bnd, ctx);
        timely_coro_yield(ctx);
        _merge(numbers, left_bnd, middle, right_bnd);
    }
}

static void
merge_sort(int numbers[], const int num_count, struct my_context * const ctx)
{
    _merge_sort(numbers, 0, num_count - 1, ctx);
}

static void
_sorted_arrays_merge(int * const sorted_arrays[], int array_lens[],
                     const int left_bnd, const int middle, const int right_bnd)
{
    const int arrays_left_batch_size = middle - left_bnd + 1;
    const int arrays_right_batch_size = right_bnd - middle;

    int left_batch_size = 0, right_batch_size = 0;
    for (int i = 0; i < arrays_left_batch_size; i++)
        left_batch_size += array_lens[left_bnd + i];
    for (int i = 0; i < arrays_right_batch_size; i++)
        right_batch_size += array_lens[middle + 1 + i];

    int left_batch[left_batch_size], right_batch[right_batch_size];
    int batch_i = 0;
    for (int arrays_i = 0; arrays_i < arrays_left_batch_size; arrays_i++) {
        for (int array_i = 0; array_i < array_lens[left_bnd + arrays_i]; array_i++) {
            left_batch[batch_i] = sorted_arrays[left_bnd + arrays_i][array_i];
            batch_i++;
        }
    }
    batch_i = 0;
    for (int arrays_i = 0; arrays_i < arrays_right_batch_size; arrays_i++) {
        for (int array_i = 0; array_i < array_lens[middle + 1 + arrays_i]; array_i++) {
            right_batch[batch_i] = sorted_arrays[middle + 1 + arrays_i][array_i];
            batch_i++;
        }
    }

    int left_batch_i = 0, right_batch_i = 0;
    int outer_replace_i = left_bnd;
    int inner_replace_i = 0;
    while (left_batch_i < left_batch_size && right_batch_i < right_batch_size) {
        if (inner_replace_i >= array_lens[outer_replace_i]) {
            outer_replace_i += 1;
            inner_replace_i = 0;
        }
        if (left_batch[left_batch_i] <= right_batch[right_batch_i]) {
            sorted_arrays[outer_replace_i][inner_replace_i] = left_batch[left_batch_i];
            left_batch_i++;
        }
        else {
            sorted_arrays[outer_replace_i][inner_replace_i] = right_batch[right_batch_i];
            right_batch_i++;
        }
        inner_replace_i++;
    }

    while (left_batch_i < left_batch_size) {
        if (inner_replace_i >= array_lens[outer_replace_i]) {
            outer_replace_i += 1;
            inner_replace_i = 0;
        }
        sorted_arrays[outer_replace_i][inner_replace_i] = left_batch[left_batch_i];
        left_batch_i++;
        inner_replace_i++;
    }
    while (right_batch_i < right_batch_size) {
        if (inner_replace_i >= array_lens[outer_replace_i]) {
            outer_replace_i += 1;
            inner_replace_i = 0;
        }
        sorted_arrays[outer_replace_i][inner_replace_i] = right_batch[right_batch_i];
        right_batch_i++;
        inner_replace_i++;
    }
}

static void
_sorted_arrays_merge_sort(int * const sorted_arrays[], int array_lens[], const int left_bnd, const int right_bnd)
{
    if (left_bnd < right_bnd) {
        const int middle = left_bnd + (right_bnd - left_bnd) / 2;
        _sorted_arrays_merge_sort(sorted_arrays, array_lens, left_bnd, middle);
        _sorted_arrays_merge_sort(sorted_arrays, array_lens, middle + 1, right_bnd);
        _sorted_arrays_merge(sorted_arrays, array_lens, left_bnd, middle, right_bnd);
    }
}

static void
sorted_arrays_merge_sort(int *sorted_arrays[], int array_lens[], const int arrays_num)
{
    _sorted_arrays_merge_sort(sorted_arrays, array_lens, 0, arrays_num - 1);
}

static int
read_file(const char * const filename, int ** const storage, int * const storage_size)
{
    FILE *r_file_ptr = fopen(filename, "r");
    if (r_file_ptr == NULL)
        return 1;
    int num_count = 0;
    int num;
    while (fscanf(r_file_ptr, "%d", &num) == 1) {
        num_count += 1;
    }
    rewind(r_file_ptr);
    /* Do not handle memory insufficiency due to the relaxation */
    int *numbers = malloc(num_count * sizeof(int));
    int i = 0;
    while (fscanf(r_file_ptr, "%d", &num) == 1) {
        numbers[i] = num;
        i += 1;
    }
    fclose(r_file_ptr);

    *storage_size = num_count;
    *storage = numbers;

    return 0;
}

static int
coroutine_func_f(void *context)
{
	struct my_context * const ctx = context;
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
	ctx->time_counter_start = start_time;

	for (int i = 0; i < ctx->arrays_num; i++) {
	    int file_is_pending = !ctx->file_statuses[i];
	    if (file_is_pending) {
            ctx->file_statuses[i] = 1;
	        char * filename = ctx->filenames[i];
            int status = read_file(filename, &ctx->storages[i], &ctx->storage_sizes[i]);
            if (status) {
                my_context_delete(ctx);
                return status;
            }
            merge_sort(ctx->storages[i], ctx->storage_sizes[i], context);
        }
    }

	my_context_delete(ctx);

	return 0;
}

int
main(int argc, char **argv)
{
    const int FILENAMES_ARG_OFFSET = 3;
    const char * const OUTPUT_FILENAME = "sorted_merged.txt";
    const long target_latency_us = atoi(argv[1]);
    const int coro_num = atoi(argv[2]);

    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
	coro_sched_init();

    const int arrays_num = argc - FILENAMES_ARG_OFFSET;
    int *sorted_arrays[arrays_num];
    int array_lens[arrays_num];
    int file_statuses[arrays_num];
    for (int i = 0; i < arrays_num; i++)
        file_statuses[i] = 0;
    const long coro_target_latency_us = target_latency_us / coro_num;
    double coro_work_times[coro_num];
    for (int coro_i = 0; coro_i < coro_num; coro_i++) {
		char **filenames = &argv[FILENAMES_ARG_OFFSET];
		coro_new(coroutine_func_f, my_context_new(filenames, sorted_arrays, array_lens, &coro_work_times[coro_i],
                                                  coro_target_latency_us, arrays_num, file_statuses));
	}
	long long coro_switch_counts[coro_num];
	struct coro *c;
	int coro_i = 0;
	while ((c = coro_sched_wait()) != NULL) {
        coro_switch_counts[coro_i] = coro_switch_count(c);
		coro_delete(c);
        coro_i++;
	}

    sorted_arrays_merge_sort(sorted_arrays, array_lens, arrays_num);

    FILE *w_file_ptr = fopen(OUTPUT_FILENAME, "w");
    if (w_file_ptr == NULL)
        return 1;
    for (int i = 0; i < arrays_num; i++) {
        for (int k = 0; k < array_lens[i]; k++) {
            fprintf(w_file_ptr, "%d ", sorted_arrays[i][k]);
        }
    }
    fclose(w_file_ptr);

    for (int i = 0; i < arrays_num; i++) {
        free(sorted_arrays[i]);
    }

    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    printf("Total work time: %lf msec\n",
           (end_time.tv_sec - start_time.tv_sec) * 1.0e3 + (end_time.tv_nsec-start_time.tv_nsec) / 1.0e6);
    for (int i = 0; i < coro_num; i++) {
        printf("Coro_%d work time: %lf msec\n", i, coro_work_times[i]);
        printf("Coro_%d switch count: %lld\n", i, coro_switch_counts[i]);
    }

	return 0;
}
