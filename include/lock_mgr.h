#ifndef LOCK_MGR_H
#define LOCK_MGR_H

#include "bank.h"
#include <stdbool.h>

// Deadlock strategy chosen at startup ("prevention" or "detection")
extern char deadlock_strategy[32];

// Thread-local transaction ID of the currently executing transaction.
// Set to -1 for the main/timer thread.
extern __thread int current_tx_id;

// Initialise the lock manager (call once before spawning threads)
void lock_mgr_init(void);

// Register a transaction before its thread starts executing operations.
void init_tx_lock_state(int tx_id);

// Drop all records for a completed/aborted transaction.
void cleanup_tx_lock_state(int tx_id);

// Acquire a write lock on an account.
// Returns false if the current transaction should abort (deadlock victim).
bool acquire_write_lock(Account* acc);

// Release a write lock held on an account.
void release_write_lock(Account* acc);

// Acquire a read lock on an account.
// Returns false if the current transaction should abort (deadlock victim).
bool acquire_read_lock(Account* acc);

// Release a read lock held on an account.
void release_read_lock(Account* acc);

#endif  // LOCK_MGR_H