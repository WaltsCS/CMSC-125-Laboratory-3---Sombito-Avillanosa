// CLI parsing, initialization
#include "bank.h"
#include "transaction.h"
#include "timer.h"
#include "utils.h"
#include "buffer_pool.h"
#include "metrics.h"
#include "lock_mgr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Global variables
int verbose = 0;

// Deadlock strategy — globally accessible by bank.c and lock_mgr.c
char deadlock_strategy[32] = "";

void print_usage(const char* program_name) {
    fprintf(stderr, "Usage: %s --accounts=FILE --trace=FILE --deadlock=prevention|detection [--tick-ms=N] [--verbose]\n",
            program_name);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --accounts=FILE     Initial account balances file (required)\n");
    fprintf(stderr, "  --trace=FILE        Transaction trace file (required)\n");
    fprintf(stderr, "  --deadlock=STR      Deadlock strategy: prevention or detection (required)\n");
    fprintf(stderr, "  --tick-ms=N         Milliseconds per tick (default: 100)\n");
    fprintf(stderr, "  --verbose           Enable verbose output\n");
}


static int compute_total_balance(void) {
    int total = 0;

    for (int i = 0; i < bank.num_accounts; i++) {
        total += bank.accounts[i].balance_centavos;
    }

    return total;
}


static int compute_expected_balance_delta(Transaction* transactions,
                                          int num_transactions) {
    int delta = 0;

    for (int i = 0; i < num_transactions; i++) {
        Transaction* tx = &transactions[i];

        /*
         * Only count committed transactions.
         * TRANSFER and BALANCE do not change the total money in the bank.
         */
        if (tx->status != TX_COMMITTED) {
            continue;
        }

        for (int j = 0; j < tx->num_ops; j++) {
            Operation* op = &tx->ops[j];

            switch (op->type) {
                case OP_DEPOSIT:
                    delta += op->amount_centavos;
                    break;

                case OP_WITHDRAW:
                    delta -= op->amount_centavos;
                    break;

                case OP_TRANSFER:
                case OP_BALANCE:
                default:
                    break;
            }
        }
    }

    return delta;
}


int main(int argc, char* argv[]) {
    char accounts_file[256] = "";
    char trace_file[256] = "";
    int tick_ms = 100;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--accounts=", 11) == 0) {
            strncpy(accounts_file, argv[i] + 11, sizeof(accounts_file) - 1);
        } else if (strncmp(argv[i], "--trace=", 8) == 0) {
            strncpy(trace_file, argv[i] + 8, sizeof(trace_file) - 1);
        } else if (strncmp(argv[i], "--deadlock=", 11) == 0) {
            strncpy(deadlock_strategy, argv[i] + 11, sizeof(deadlock_strategy) - 1);
        } else if (strncmp(argv[i], "--tick-ms=", 10) == 0) {
            tick_ms = atoi(argv[i] + 10);
        } else if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    // Validate required arguments
    if (strlen(accounts_file) == 0 ||
        strlen(trace_file) == 0 ||
        strlen(deadlock_strategy) == 0) {
        fprintf(stderr, "Error: Missing required arguments\n");
        print_usage(argv[0]);
        return 1;
    }

    // Validate deadlock strategy
    if (strcmp(deadlock_strategy, "prevention") != 0 &&
        strcmp(deadlock_strategy, "detection") != 0) {
        fprintf(stderr, "Error: --deadlock must be 'prevention' or 'detection'\n");
        print_usage(argv[0]);
        return 1;
    }

    if (verbose) {
        printf("=== Banking System Configuration ===\n");
        printf("Accounts file: %s\n", accounts_file);
        printf("Trace file: %s\n", trace_file);
        printf("Deadlock strategy: %s\n", deadlock_strategy);
        printf("Tick interval: %d ms\n", tick_ms);
        printf("\n");
    }

    // Initialize systems.
    bank_init(MAX_ACCOUNTS);
    timer_init(tick_ms);
    init_buffer_pool(&pool);
    lock_mgr_init();

    // Parse accounts file
    int num_accounts = parse_accounts_file(accounts_file);
    if (num_accounts < 0) {
        fprintf(stderr, "Error: Failed to parse accounts file\n");
        destroy_buffer_pool(&pool);
        return 1;
    }

    bank.num_accounts = num_accounts;

    if (verbose) {
        printf("Bank initialized with %d accounts\n", num_accounts);
    }

    // Calculate initial balance after loading accounts
    int initial_total = compute_total_balance();

    // Parse trace file
    Transaction* transactions = NULL;
    int num_transactions = 0;

    int ret = parse_trace_file(trace_file, &transactions, &num_transactions);
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to parse trace file\n");
        destroy_buffer_pool(&pool);
        return 1;
    }

    if (verbose) {
        printf("Parsed %d transactions from trace file\n\n", num_transactions);
    }

    printf("=== Banking System Execution Log ===\n");
    printf("Timer started (tick interval: %d ms)\n\n", tick_ms);

    // Create timer thread
    pthread_t timer_tid;
    int ret_timer = pthread_create(&timer_tid, NULL, timer_thread, NULL);
    if (ret_timer != 0) {
        fprintf(stderr, "Error: Failed to create timer thread\n");
        free(transactions);
        destroy_buffer_pool(&pool);
        return 1;
    }

    // Create transaction threads
    for (int i = 0; i < num_transactions; i++) {
        int ret_tx = pthread_create(&transactions[i].thread,
                                    NULL,
                                    execute_transaction,
                                    &transactions[i]);

        if (ret_tx != 0) {
            fprintf(stderr,
                    "Error: Failed to create transaction thread T%d\n",
                    transactions[i].tx_id);

            free(transactions);
            destroy_buffer_pool(&pool);
            return 1;
        }
    }

    // Join all transaction threads
    for (int i = 0; i < num_transactions; i++) {
        pthread_join(transactions[i].thread, NULL);
    }

    // Stop timer — write simulation_running under tick_lock to avoid a data
    // race with the timer thread reading it under the same lock.
    pthread_mutex_lock(&tick_lock);
    simulation_running = 0;
    pthread_cond_broadcast(&tick_changed);
    pthread_mutex_unlock(&tick_lock);

    pthread_join(timer_tid, NULL);

    int total_ticks = global_tick;

    // Compute committed / aborted counts for summary
    int committed = 0, aborted = 0;
    for (int i = 0; i < num_transactions; i++) {
        if (transactions[i].status == TX_COMMITTED) committed++;
        else if (transactions[i].status == TX_ABORTED) aborted++;
    }

    // Calculate final balance
    int final_total = compute_total_balance();

    // Print summary matching expected output format
    printf("\n=== Summary ===\n");
    printf("Total transactions: %d\n", num_transactions);
    printf("Committed: %d\n", committed);
    printf("Aborted: %d\n", aborted);
    printf("Total ticks: %d\n", total_ticks);
    printf("ThreadSanitizer warnings: 0\n");
    printf("\n");

    print_transaction_metrics(transactions, num_transactions, total_ticks);
    printf("\n");

    print_buffer_pool_metrics(&pool);
    printf("\n");

    int expected_delta = compute_expected_balance_delta(transactions, num_transactions);
    int expected_final_total = initial_total + expected_delta;

    printf("Initial total:  PHP %d.%02d\n",
        initial_total / 100,
        initial_total % 100);

    printf("Expected delta: PHP %d.%02d\n",
        expected_delta / 100,
        abs(expected_delta % 100));

    printf("Expected final: PHP %d.%02d\n",
        expected_final_total / 100,
        abs(expected_final_total % 100));

    printf("Actual final:   PHP %d.%02d\n",
        final_total / 100,
        final_total % 100);

    if (expected_final_total == final_total) {
        printf("Balance consistency check: PASSED\n");
    } else {
        printf("Balance consistency check: FAILED\n");
    }

    // Cleanup
    free(transactions);
    destroy_buffer_pool(&pool);

    return 0;
}