#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define ITERATIONS 100
#define STEP 0.001
#define WIDTH 3001
#define HEIGHT 3001

typedef struct {
    double x;
    double y;
    double time;
    int thread_id;
} Point;

Point *points;
int counter = 0;
int npoints;
int nthreads;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int is_mandelbrot(double x, double y) {
    double a = 0;
    double b = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        double new_a = a * a - b * b + x;
        double new_b = 2 * a * b + y;

        a = new_a;
        b = new_b;

        if (a * a + b * b > 4)
            return 0;
    }

    return 1;
}

double time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) +
           (end.tv_nsec - start.tv_nsec) / 1e9;
}

void *worker(void *arg) {
    int id = *(int *)arg;

    for (long long iteration = 0; ; iteration++) {
        struct timespec start, end;

        clock_gettime(CLOCK_MONOTONIC, &start);

        long long index = iteration * nthreads + id;

        if (index >= (long long)WIDTH * HEIGHT)
            break;

        pthread_mutex_lock(&mutex);
        int finished = counter >= npoints;
        pthread_mutex_unlock(&mutex);

        if (finished)
            break;

        double x = -2.0 + (index % WIDTH) * STEP;
        double y = -1.5 + (index / WIDTH) * STEP;
        
        int result = is_mandelbrot(x, y);
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (result) {
            pthread_mutex_lock(&mutex);

            if (counter < npoints) {
                points[counter].x = x;
                points[counter].y = y;
                points[counter].time = time_diff(start, end);
                points[counter].thread_id = id;

                counter++;
            }

            pthread_mutex_unlock(&mutex);
        }
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    nthreads = atoi(argv[1]);
    npoints = atoi(argv[2]);

    points = malloc((size_t)npoints * sizeof(Point));

    pthread_t *threads = malloc(nthreads * sizeof(pthread_t));
    int *ids = malloc(nthreads * sizeof(int));

    if (threads == NULL || ids == NULL) {
        free(points);
        free(threads);
        free(ids);
        return 1;
    }

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < nthreads; i++) {
        ids[i] = i;
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double total_time = time_diff(start, end);

    FILE *file = fopen("points.csv", "w");

    fprintf(file, "x,y,thread_id,check_time\n");

    for (int i = 0; i < counter; i++) {
        fprintf(file, "%.6f,%.6f,%d,%.9f\n",
                points[i].x,
                points[i].y,
                points[i].thread_id,
                points[i].time);
    }

    fprintf(file, "\n");
    fprintf(file, "nthreads,npoints,total_time\n");
    fprintf(file, "%d,%d,%.9f\n",
            nthreads, counter, total_time);

    fclose(file);

    free(points);
    free(threads);
    free(ids);

    pthread_mutex_destroy(&mutex);

    return 0;
}