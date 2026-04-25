#ifndef TIMER_H
#define TIMER_H

#include <pthread.h>

// Global simulation clock
extern volatile int global_tick;
extern pthread_mutex_t tick_lock;
extern pthread_cond_t tick_changed;

// Configuration
extern int tick_interval_ms;
extern int simulation_running;

// Function declarations
void timer_init(int interval_ms);
void* timer_thread(void* arg);
void wait_until_tick(int target_tick);

#endif // TIMER_H