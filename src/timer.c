// Timer thread implementation
#include "timer.h"
#include <stdio.h>
#include <unistd.h>

volatile int global_tick = 0;
pthread_mutex_t tick_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t tick_changed = PTHREAD_COND_INITIALIZER;

int tick_interval_ms = 100;  // Default
int simulation_running = 1;

void timer_init(int interval_ms) {
    tick_interval_ms = interval_ms;
    global_tick = 0;
    simulation_running = 1;
}

void* timer_thread(void* arg) {
    (void)arg;  // suppress unused parameter warning

    while (1) {
        usleep(tick_interval_ms * 1000);  // Convert ms to microseconds

        pthread_mutex_lock(&tick_lock);

        // Check the flag while holding tick_lock to avoid a data race.
        if (!simulation_running) {
            pthread_mutex_unlock(&tick_lock);
            break;
        }

        global_tick++;
        pthread_cond_broadcast(&tick_changed);
        pthread_mutex_unlock(&tick_lock);
    }

    return NULL;
}

void wait_until_tick(int target_tick) {
    pthread_mutex_lock(&tick_lock);
    while (global_tick < target_tick) {
        pthread_cond_wait(&tick_changed, &tick_lock);
    }
    pthread_mutex_unlock(&tick_lock);
}