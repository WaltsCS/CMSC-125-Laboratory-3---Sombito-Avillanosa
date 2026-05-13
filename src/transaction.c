// Transaction execution thread
#include "transaction.h"
#include "bank.h"
#include "timer.h"
#include "buffer_pool.h"

#include <stdio.h>

static int read_current_tick(void) {
    int tick;

    pthread_mutex_lock(&tick_lock);
    tick = global_tick;
    pthread_mutex_unlock(&tick_lock);

    return tick;
}

void* execute_transaction(void* arg) {
    Transaction* tx = (Transaction*)arg;

    if (tx == NULL) {
        return NULL;
    }

    // Wait until the scheduled simulation tick before starting.
    wait_until_tick(tx->start_tick);

    tx->actual_start = read_current_tick();
    tx->actual_end = tx->actual_start;
    tx->wait_ticks = 0;
    tx->status = TX_RUNNING;

    printf("Tick %d: T%d started\n", tx->actual_start, tx->tx_id);


    // Load all accounts needed by this transaction into the buffer pool.
    // For TRANSFER, both source and target accounts are loaded.
    
    for (int i = 0; i < tx->num_ops; i++) {
        Operation* op = &tx->ops[i];

        load_account(&pool, op->account_id);

        if (op->type == OP_TRANSFER) {
            load_account(&pool, op->target_account);
        }
    }

    
    // Execute each operation in order.

    for (int i = 0; i < tx->num_ops; i++) {
        Operation* op = &tx->ops[i];

        int tick_before = read_current_tick();
        int success = 1;

        switch (op->type) {
            case OP_DEPOSIT:
                printf("T%d: DEPOSIT account %d amount PHP %d.%02d\n",
                       tx->tx_id,
                       op->account_id,
                       op->amount_centavos / 100,
                       op->amount_centavos % 100);

                success = deposit(op->account_id, op->amount_centavos);
                break;

            case OP_WITHDRAW:
                printf("T%d: WITHDRAW account %d amount PHP %d.%02d\n",
                       tx->tx_id,
                       op->account_id,
                       op->amount_centavos / 100,
                       op->amount_centavos % 100);

                success = withdraw(op->account_id, op->amount_centavos);

                if (!success) {
                    printf("T%d aborted: insufficient funds or invalid withdrawal\n",
                           tx->tx_id);
                }

                break;

            case OP_TRANSFER:
                printf("T%d: TRANSFER from account %d to account %d amount PHP %d.%02d\n",
                       tx->tx_id,
                       op->account_id,
                       op->target_account,
                       op->amount_centavos / 100,
                       op->amount_centavos % 100);

                success = transfer(op->account_id,
                                   op->target_account,
                                   op->amount_centavos);

                if (!success) {
                    printf("T%d aborted: transfer failed\n", tx->tx_id);
                }

                break;

            case OP_BALANCE: {
                int balance = get_balance(op->account_id);

                if (balance < 0) {
                    printf("T%d: BALANCE failed for account %d\n",
                           tx->tx_id,
                           op->account_id);
                    success = 0;
                } else {
                    printf("T%d: Account %d balance = PHP %d.%02d\n",
                           tx->tx_id,
                           op->account_id,
                           balance / 100,
                           balance % 100);
                }

                break;
            }

            default:
                printf("T%d aborted: unknown operation\n", tx->tx_id);
                success = 0;
                break;
        }

        int tick_after = read_current_tick();
        tx->wait_ticks += tick_after - tick_before;

        if (!success) {
            tx->status = TX_ABORTED;
            tx->actual_end = read_current_tick();

            // Unload accounts before exiting early.
            for (int j = 0; j < tx->num_ops; j++) {
                Operation* cleanup_op = &tx->ops[j];

                unload_account(&pool, cleanup_op->account_id);

                if (cleanup_op->type == OP_TRANSFER) {
                    unload_account(&pool, cleanup_op->target_account);
                }
            }

            return NULL;
        }
    }

    
    // Unload all accounts after successful execution.
    for (int i = 0; i < tx->num_ops; i++) {
        Operation* op = &tx->ops[i];

        unload_account(&pool, op->account_id);

        if (op->type == OP_TRANSFER) {
            unload_account(&pool, op->target_account);
        }
    }

    tx->actual_end = read_current_tick();
    tx->status = TX_COMMITTED;

    printf("Tick %d: T%d committed\n", tx->actual_end, tx->tx_id);

    return NULL;
}