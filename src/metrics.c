// Metrics calculation and reporting
#include "metrics.h"
#include <stdio.h>

void print_transaction_metrics(Transaction* transactions, int num_transactions, int total_ticks) {
    printf("=== Transaction Performance Metrics ===\n");
    printf("TxID | StartTick | ActualStart | End | WaitTicks | Status\n");
    printf("-----|-----------|-------------|-----|-----------|----------\n");
    
    int committed = 0;
    int aborted = 0;
    int total_wait = 0;
    
    for (int i = 0; i < num_transactions; i++) {
        Transaction* tx = &transactions[i];
        const char* status_str = (tx->status == TX_COMMITTED) ? "COMMITTED" : 
                                 (tx->status == TX_ABORTED ? "ABORTED" : "RUNNING");
        
        if (tx->status == TX_COMMITTED) committed++;
        else if (tx->status == TX_ABORTED) aborted++;
        
        total_wait += tx->wait_ticks;
        
        printf("T%-3d | %9d | %11d | %3d | %9d | %s\n",
               tx->tx_id, tx->start_tick, tx->actual_start, tx->actual_end, tx->wait_ticks, status_str);
    }
    
    double avg_wait = (double)total_wait / num_transactions;
    double throughput = (double)num_transactions / total_ticks;
    
    printf("\nAverage wait time: %.1f ticks\n", avg_wait);
    printf("Throughput: %d transactions / %d ticks = %.2f tx/tick\n", 
           num_transactions, total_ticks, throughput);
}

void print_buffer_pool_metrics(BufferPool* p) {
    printf("=== Buffer Pool Report ===\n");
    printf("Pool size: %d slots\n", BUFFER_POOL_SIZE);
    printf("Total loads: %d\n", p->total_loads);
    printf("Total unloads: %d\n", p->total_unloads);
    printf("Peak usage: %d slots\n", p->peak_usage);
    printf("Blocked operations (pool full): %d\n", p->blocked_operations);
}