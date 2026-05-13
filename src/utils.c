// Parsing, error handling
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_error(const char* message) {
    fprintf(stderr, "ERROR: %s\n", message);
}

void print_info(const char* message) {
    printf("%s\n", message);
}

int parse_accounts_file(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open accounts file: %s\n", filename);
        return -1;
    }
    
    int account_count = 0;
    char line[256];
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' || (line[0] == '/' && line[1] == '/')) {
            continue;
        }
        
        int account_id, balance_centavos;
        int ret = sscanf(line, "%d %d", &account_id, &balance_centavos);
        
        if (ret != 2) {
            fprintf(stderr, "Error: Invalid line in accounts file: %s", line);
            fclose(fp);
            return -1;
        }
        
        // Store account in bank
        bank.accounts[account_count].account_id = account_id;
        bank.accounts[account_count].balance_centavos = balance_centavos;
        account_count++;
    }
    
    fclose(fp);
    return account_count;
}

int parse_trace_file(const char* filename, Transaction** transactions, int* num_transactions) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error: Cannot open trace file: %s\n", filename);
        return -1;
    }

    // Allocate array for transactions (max 1000)
    *transactions = (Transaction*)malloc(sizeof(Transaction) * 1000);
    if (*transactions == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(fp);
        return -1;
    }

    int tx_count = 0;
    int current_tx_id = -1;

    char line[256];

    while (fgets(line, sizeof(line), fp) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r' ||
            (line[0] == '/' && line[1] == '/')) {
            continue;
        }

        int tx_id;
        int start_tick;
        char op_type_str[16];

        /*
         * First, read only the common fields:
         * TxID, StartTick, and Operation Type.
         */
        int ret = sscanf(line, "T%d %d %15s",
                         &tx_id,
                         &start_tick,
                         op_type_str);

        if (ret != 3) {
            fprintf(stderr, "Error: Invalid line in trace file: %s", line);
            fclose(fp);
            free(*transactions);
            return -1;
        }

        Operation op;
        op.account_id = -1;
        op.target_account = -1;
        op.amount_centavos = 0;

        /*
         * Parse each operation type separately because each one has
         * a different number/order of fields.
         */
        if (strcmp(op_type_str, "DEPOSIT") == 0) {
            int account_id;
            int amount;

            ret = sscanf(line, "T%d %d %15s %d %d",
                         &tx_id,
                         &start_tick,
                         op_type_str,
                         &account_id,
                         &amount);

            if (ret != 5) {
                fprintf(stderr, "Error: Invalid DEPOSIT line: %s", line);
                fclose(fp);
                free(*transactions);
                return -1;
            }

            op.type = OP_DEPOSIT;
            op.account_id = account_id;
            op.amount_centavos = amount;
            op.target_account = -1;

        } else if (strcmp(op_type_str, "WITHDRAW") == 0) {
            int account_id;
            int amount;

            ret = sscanf(line, "T%d %d %15s %d %d",
                         &tx_id,
                         &start_tick,
                         op_type_str,
                         &account_id,
                         &amount);

            if (ret != 5) {
                fprintf(stderr, "Error: Invalid WITHDRAW line: %s", line);
                fclose(fp);
                free(*transactions);
                return -1;
            }

            op.type = OP_WITHDRAW;
            op.account_id = account_id;
            op.amount_centavos = amount;
            op.target_account = -1;

        } else if (strcmp(op_type_str, "TRANSFER") == 0) {
            int from_id;
            int target_account;
            int amount;

            /*
             * Required format:
             * T1 0 TRANSFER from_id target_account amount
             */
            ret = sscanf(line, "T%d %d %15s %d %d %d",
                         &tx_id,
                         &start_tick,
                         op_type_str,
                         &from_id,
                         &target_account,
                         &amount);

            if (ret != 6) {
                fprintf(stderr, "Error: Invalid TRANSFER line: %s", line);
                fclose(fp);
                free(*transactions);
                return -1;
            }

            op.type = OP_TRANSFER;
            op.account_id = from_id;
            op.target_account = target_account;
            op.amount_centavos = amount;

        } else if (strcmp(op_type_str, "BALANCE") == 0) {
            int account_id;

            ret = sscanf(line, "T%d %d %15s %d",
                         &tx_id,
                         &start_tick,
                         op_type_str,
                         &account_id);

            if (ret != 4) {
                fprintf(stderr, "Error: Invalid BALANCE line: %s", line);
                fclose(fp);
                free(*transactions);
                return -1;
            }

            op.type = OP_BALANCE;
            op.account_id = account_id;
            op.amount_centavos = 0;
            op.target_account = -1;

        } else {
            fprintf(stderr, "Error: Unknown operation type: %s\n", op_type_str);
            fclose(fp);
            free(*transactions);
            return -1;
        }

        // Start new transaction if different ID
        if (tx_id != current_tx_id) {
            if (current_tx_id != -1) {
                tx_count++;
            }

            current_tx_id = tx_id;

            (*transactions)[tx_count].tx_id = tx_id;
            (*transactions)[tx_count].start_tick = start_tick;
            (*transactions)[tx_count].num_ops = 0;
            (*transactions)[tx_count].actual_start = -1;
            (*transactions)[tx_count].actual_end = -1;
            (*transactions)[tx_count].wait_ticks = 0;
            (*transactions)[tx_count].status = TX_RUNNING;
        }

        // Add operation to current transaction
        int op_idx = (*transactions)[tx_count].num_ops;

        if (op_idx >= 256) {
            fprintf(stderr, "Error: Too many operations in transaction T%d\n", tx_id);
            fclose(fp);
            free(*transactions);
            return -1;
        }

        (*transactions)[tx_count].ops[op_idx] = op;
        (*transactions)[tx_count].num_ops++;
    }

    // Count final transaction
    if (current_tx_id != -1) {
        tx_count++;
    }

    *num_transactions = tx_count;
    fclose(fp);
    return 0;  // Success
}