// Transaction execution thread
#include "transaction.h"
#include <stdio.h>

void* execute_transaction(void* arg) {
    Transaction* tx = (Transaction*)arg;
    
    printf("T%d: Transaction started\n", tx->tx_id);
    
    // Week 2: Will implement actual operations here
    
    tx->status = TX_COMMITTED;
    return NULL;
}