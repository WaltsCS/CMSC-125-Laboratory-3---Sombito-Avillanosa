#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include "bank.h"
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define BUFFER_POOL_SIZE 5

typedef struct {
    int account_id;
    Account* data;
    bool in_use;
} BufferSlot;

typedef struct {
    BufferSlot slots[BUFFER_POOL_SIZE];
    sem_t empty_slots;
    sem_t full_slots;
    pthread_mutex_t pool_lock;
    
    // Metrics
    int total_loads;
    int total_unloads;
    int peak_usage;
    int blocked_operations;
} BufferPool;

extern BufferPool pool;

void init_buffer_pool(BufferPool* p);
void destroy_buffer_pool(BufferPool* p);    // clean up init'd semaphores and mutexes

void load_account(BufferPool* p, int account_id);
void unload_account(BufferPool* p, int account_id);

#endif  // BUFFER_POOL_HEADER