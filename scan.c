#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_LINE_SIZE 256

typedef struct __shared_thread_args {
    int *elements;
    int *psums;
    int thread_completed_count;
    int elements_count;
    int thread_count;
    int offset;
} shared_thread_args;

typedef struct __local_thread_args {
    shared_thread_args *args;
    int thread_id;
} local_thread_args;

typedef struct {
    int thread_count;
    int count;
    sem_t lock;
    sem_t stop_1;
    sem_t stop_2;
} barrier_t;

barrier_t barrier;
pthread_mutex_t lock;

void init_barrier(barrier_t *barrier, int thread_count) {
    barrier->thread_count = thread_count;
    barrier->count = 0;
    sem_init(&barrier->lock, 0, 1);
    sem_init(&barrier->stop_1, 0, 0);
    sem_init(&barrier->stop_2, 0, 0);
}

void barrier_1(barrier_t *barrier) {
    sem_wait(&barrier->lock);
    if (++barrier->count == barrier->thread_count) {
        int i;
        for (i = 0; i < barrier->thread_count; i++) {
            sem_post(&barrier->stop_1);
        }
    }
    sem_post(&barrier->lock);
    sem_wait(&barrier->stop_1);
}

void barrier_2(barrier_t *barrier) {
    sem_wait(&barrier->lock);
    if (--barrier->count == 0) {
        int i;
        for (i = 0; i < barrier->thread_count; i++) {
            sem_post(&barrier->stop_2);
        }
    }
    sem_post(&barrier->lock);
    sem_wait(&barrier->stop_2);
}

void wait_barrier(barrier_t *barrier) {
    barrier_1(barrier);
    barrier_2(barrier);
}

void read_input_vector(const char *filename, int n, int *array) {
    FILE *fp;
    char *line = malloc(MAX_LINE_SIZE + 1);
    size_t len = MAX_LINE_SIZE;

    fp = strcmp(filename, "-") ? fopen(filename, "r") : stdin;

    assert(fp != NULL && line != NULL);

    int index = 0;

    while (getline(&line, &len, fp) != -1) {
        array[index] = atoi(line);
        index++;
    }

    free(line);
    fclose(fp);
}

void *inclusive_scan(void *raw_ags) {
    local_thread_args *_local_thread_args = (local_thread_args *) raw_ags;
    shared_thread_args *args = _local_thread_args->args;
    int thread_num = _local_thread_args->thread_id;
    int elements_count = args->elements_count;
    int local_step = 0;
    while (1) {
        pthread_mutex_lock(&lock);
        if (local_step != 0 &&
            args->thread_completed_count == args->thread_count * (local_step)) {
            memcpy(args->elements, args->psums, sizeof(int) * elements_count);
        }
        args->thread_completed_count++;
        pthread_mutex_unlock(&lock);
        int *elements = args->elements;
        int offset = args->offset;
        int current_step = local_step;
        int start_index = thread_num * offset;
        int current_step_offset = pow(2, current_step);
        for (int i = start_index; i < elements_count && i < start_index + offset; i++) {
            if (i - current_step_offset >= 0) {
                args->psums[i] = elements[i] + elements[i - current_step_offset];
            }
        }
        wait_barrier(&barrier);
        local_step++;
        if (pow(2, local_step) >= elements_count) {
            args->thread_count--;
            return NULL;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <input_file> <size> <thread_count count>\n", argv[0]);
        return 1;
    }
    char *filename = argv[1];
    int element_count = atoi(argv[2]);
    int thread_count = atoi(argv[3]);
    shared_thread_args *args = malloc(sizeof(shared_thread_args));

    args->elements = malloc(element_count * sizeof(int));
    args->thread_completed_count = 0;
    args->psums = malloc(element_count * sizeof(int));
    args->elements_count = element_count;
    args->thread_count = thread_count;
    args->offset = (int) (element_count / thread_count) + (element_count % thread_count == 0 ? 0 : 1);
    read_input_vector(filename, element_count, args->elements);
    memcpy(args->psums, args->elements, sizeof(int) * element_count);
    init_barrier(&barrier, thread_count);
    pthread_mutex_init(&lock, NULL);
    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; i++) {
        local_thread_args *_local_thread_args = malloc(sizeof(local_thread_args));
        _local_thread_args->args = args;
        _local_thread_args->thread_id = i;
        pthread_create(&threads[i], NULL, inclusive_scan, _local_thread_args);
    }
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    for (int i = 0; i < element_count; i++) {
        printf("%d\n", args->psums[i]);
    }
}