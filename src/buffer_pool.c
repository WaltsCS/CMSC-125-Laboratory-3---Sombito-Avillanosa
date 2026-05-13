// Bounded buffer implementation
#include "buffer_pool.h"
#include <stdio.h>

BufferPool pool;

// creates sync resources & init buffer pool
void init_buffer_pool(BufferPool* p) {
    sem_init(&p->empty_slots, 0, BUFFER_POOL_SIZE);
    sem_init(&p->full_slots, 0, 0);
    pthread_mutex_init(&p->pool_lock, NULL);
    
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        p->slots[i].in_use = false;
        p->slots[i].account_id = -1;
        p->slots[i].data = NULL;
    }
    
    p->total_loads = 0;
    p->total_unloads = 0;
    p->peak_usage = 0;
    p->blocked_operations = 0;
}

// clean up sync resources
void destroy_buffer_pool(BufferPool* p) {
    if (p == NULL) {
        return;
    }

    sem_destroy(&p->empty_slots);
    sem_destroy(&p->full_slots);
    pthread_mutex_destroy(&p->pool_lock);
}


void load_account(BufferPool* p, int account_id) {
    // Check if wait will block (for metrics)
    int sem_val;
    sem_getvalue(&p->empty_slots, &sem_val);
    if (sem_val <= 0) {
        pthread_mutex_lock(&p->pool_lock);
        p->blocked_operations++;
        pthread_mutex_unlock(&p->pool_lock);
    }

    sem_wait(&p->empty_slots); // Wait for empty slot

    pthread_mutex_lock(&p->pool_lock);
    
    // Find empty slot and load account
    int current_usage = 0;
    bool loaded = false;
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (!p->slots[i].in_use && !loaded) {
            p->slots[i].account_id = account_id;
            p->slots[i].data = get_account(account_id);
            p->slots[i].in_use = true;
            loaded = true;
        }
        if (p->slots[i].in_use) {
            current_usage++;
        }
    }
    
    p->total_loads++;
    if (current_usage > p->peak_usage) {
        p->peak_usage = current_usage;
    }
    
    pthread_mutex_unlock(&p->pool_lock);
    
    sem_post(&p->full_slots); // Signal slot is full
}


void unload_account(BufferPool* p, int account_id) {
    sem_wait(&p->full_slots); // Wait for full slot

    pthread_mutex_lock(&p->pool_lock);
    
    // Find and unload account
    for (int i = 0; i < BUFFER_POOL_SIZE; i++) {
        if (p->slots[i].in_use && p->slots[i].account_id == account_id) {
            p->slots[i].in_use = false;
            p->slots[i].account_id = -1;
            p->slots[i].data = NULL;
            break;
        }
    }
    
    p->total_unloads++;
    
    pthread_mutex_unlock(&p->pool_lock);
    
    sem_post(&p->empty_slots); // Signal slot is empty
}