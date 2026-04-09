// Timer thread and clock functions
extern volatile int global_tick;
extern int simulation_running;
extern pthread_mutex_t tick_lock;
extern pthread_cond_t tick_changed;