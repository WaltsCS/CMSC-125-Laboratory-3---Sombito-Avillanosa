// CLI parsing, initialization
#include "bank.h"
#include "transaction.h"
#include "timer.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Global variables
int verbose = 0;

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

int main(int argc, char* argv[]) {
    char accounts_file[256] = "";
    char trace_file[256] = "";
    char deadlock_strategy[32] = "";
    int tick_ms = 100;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--accounts=", 11) == 0) {
            strcpy(accounts_file, argv[i] + 11);
        } else if (strncmp(argv[i], "--trace=", 8) == 0) {
            strcpy(trace_file, argv[i] + 8);
        } else if (strncmp(argv[i], "--deadlock=", 11) == 0) {
            strcpy(deadlock_strategy, argv[i] + 11);
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
    if (strlen(accounts_file) == 0 || strlen(trace_file) == 0 || strlen(deadlock_strategy) == 0) {
        fprintf(stderr, "Error: Missing required arguments\n");
        print_usage(argv[0]);
        return 1;
    }
    
    // Validate deadlock strategy
    if (strcmp(deadlock_strategy, "prevention") != 0 && strcmp(deadlock_strategy, "detection") != 0) {
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
    
    // Initialize timer
    timer_init(tick_ms);
    
    // Parse accounts file
    int num_accounts = parse_accounts_file(accounts_file);
    if (num_accounts < 0) {
        fprintf(stderr, "Error: Failed to parse accounts file\n");
        return 1;
    }
    
    // Initialize bank with parsed account count
    bank_init(num_accounts);
    
    if (verbose) {
        printf("Bank initialized with %d accounts\n", num_accounts);
    }
    
    // Parse trace file
    Transaction* transactions;
    int num_transactions;
    int ret = parse_trace_file(trace_file, &transactions, &num_transactions);
    if (ret != 0) {
        fprintf(stderr, "Error: Failed to parse trace file\n");
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
        return 1;
    }
    
    // Create transaction threads
    for (int i = 0; i < num_transactions; i++) {
        int ret_tx = pthread_create(&transactions[i].thread, NULL, 
                                    execute_transaction, &transactions[i]);
        if (ret_tx != 0) {
            fprintf(stderr, "Error: Failed to create transaction thread T%d\n", 
                   transactions[i].tx_id);
            return 1;
        }
    }
    
    // Join all transaction threads
    for (int i = 0; i < num_transactions; i++) {
        pthread_join(transactions[i].thread, NULL);
    }
    
    // Stop timer
    simulation_running = 0;
    pthread_join(timer_tid, NULL);
    
    // Print summary
    printf("\n=== Summary ===\n");
    printf("Total transactions: %d\n", num_transactions);
    printf("Final tick: %d\n", global_tick);
    
    // Cleanup
    free(transactions);
    
    return 0;
}