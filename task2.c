#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <time.h>

#define THREADS 8
#define G 6.67430e-11
#define DT 1.0

typedef struct {
    double m, x, y, vx, vy;
} Body;

Body *bodies;
double **fx, **fy;

int n;
int steps;

FILE *output;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int waiting = 0;
int generation = 0;

double time_diff(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) +
           (end.tv_nsec - start.tv_nsec) / 1e9;
}
void barrier_wait() {
    pthread_mutex_lock(&mutex);

    int current = generation;
    waiting++;

    if (waiting == THREADS) {
        waiting = 0;
        generation++;
        pthread_cond_broadcast(&cond);
    } else {
        while (current == generation) {
            pthread_cond_wait(&cond, &mutex);
        }
    }

    pthread_mutex_unlock(&mutex);
}

void save(int t) {
    fprintf(output, "%d", t);

    for (int i = 0; i < n; i++) {
        fprintf(output, ",%.6f,%.6f", bodies[i].x, bodies[i].y);
    }

    fprintf(output, "\n");
}

void *worker(void *arg) {
    int id = *(int *)arg;

    int start = id * n / THREADS;
    int end = (id + 1) * n / THREADS;

    for (int t = 1; t <= steps; t++) {

        for (int i = 0; i < n; i++) {
            fx[id][i] = 0;
            fy[id][i] = 0;
        }

        for (int i = start; i < end; i++) {
            for (int j = i + 1; j < n; j++) {
                double dx = bodies[j].x - bodies[i].x;
                double dy = bodies[j].y - bodies[i].y;

                double r2 = dx * dx + dy * dy;

                if (r2 == 0)
                    continue;

                double r = sqrt(r2);
                double f = G * bodies[i].m * bodies[j].m / (r2 * r);

                double force_x = f * dx;
                double force_y = f * dy;

                fx[id][i] += force_x;
                fy[id][i] += force_y;

                fx[id][j] -= force_x;
                fy[id][j] -= force_y;
            }

        }

        barrier_wait();

        for (int i = start; i < end; i++) {
            double total_fx = 0;
            double total_fy = 0;

            for (int p = 0; p < THREADS; p++) {
                total_fx += fx[p][i];
                total_fy += fy[p][i];
            }

            double ax = total_fx / bodies[i].m;
            double ay = total_fy / bodies[i].m;

            bodies[i].x += bodies[i].vx * DT;
            bodies[i].y += bodies[i].vy * DT;

            bodies[i].vx += ax * DT;
            bodies[i].vy += ay * DT;
        }

        barrier_wait();

        if (id == 0) {
            save(t);
        }

        barrier_wait();
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    double tend = atof(argv[1]);

    steps = (int)(tend / DT);

    FILE *file = fopen(argv[2], "r");

    fscanf(file, "%d", &n);

    bodies = malloc(n * sizeof(Body));

    for (int i = 0; i < n; i++) {
        double z, vz;
        fscanf(file, "%lf %lf %lf %lf %lf %lf %lf",
                   &bodies[i].m,
                   &bodies[i].x,
                   &bodies[i].y,
                   &z,
                   &bodies[i].vx,
                   &bodies[i].vy,
                   &vz);
    }

    fclose(file);

    fx = malloc(THREADS * sizeof(double *));
    fy = malloc(THREADS * sizeof(double *));



    for (int i = 0; i < THREADS; i++) {
        fx[i] = calloc(n, sizeof(double));
        fy[i] = calloc(n, sizeof(double));
    }

    output = fopen("output.csv", "w");

    if (!output) {
        return 1;
    }

    fprintf(output, "t");

    for (int i = 0; i < n; i++) {
        fprintf(output, ",x%d,y%d", i + 1, i + 1);
    }

    fprintf(output, "\n");

    save(0);

    pthread_t threads[THREADS];
    int ids[THREADS];

    struct timespec time_start, time_end;

    clock_gettime(CLOCK_MONOTONIC, &time_start);

    for (int i = 0; i < THREADS; i++) {
        ids[i] = i;

        pthread_create(&threads[i], NULL, worker, &ids[i]);
    }

    for (int i = 0; i < THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &time_end);

    double total_time = time_diff(time_start, time_end);


    fprintf(output, "\n");
    fprintf(output, "nthreads,npoints,total_time\n");
    fprintf(output, "%d,%d,%.9f\n",
            THREADS, n, total_time);

    fclose(output);

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    for (int i = 0; i < THREADS; i++) {
        free(fx[i]);
        free(fy[i]);
    }

    free(fx);
    free(fy);
    free(bodies);

    return 0;
}