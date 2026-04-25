#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <pthread.h>

typedef enum {
    OP_DEPOSIT,
    OP_WITHDRAW,
    OP_TRANSFER,
    OP_BALANCE
} OpType;

typedef struct {
    OpType type;
    int account_id;
    int amount_centavos;
    int target_account;  // For TRANSFER only
} Operation;

typedef enum {
    TX_RUNNING,
    TX_COMMITTED,
    TX_ABORTED
} TxStatus;

typedef struct {
    int tx_id;
    Operation ops[256];
    int num_ops;
    int start_tick;
    
    // Timing (in ticks)
    int actual_start;
    int actual_end;
    int wait_ticks;
    
    // Status
    TxStatus status;
    
    // Thread
    pthread_t thread;
} Transaction;

// Function declarations
void* execute_transaction(void* arg);

#endif // TRANSACTION_H