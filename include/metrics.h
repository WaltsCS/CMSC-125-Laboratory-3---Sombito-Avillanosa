// Statistics collection
#ifndef METRICS_H
#define METRICS_H

#include "transaction.h"
#include "buffer_pool.h"

void print_transaction_metrics(Transaction* transactions, int num_transactions, int total_ticks);
void print_buffer_pool_metrics(BufferPool* p);

#endif // METRICS_HEADER