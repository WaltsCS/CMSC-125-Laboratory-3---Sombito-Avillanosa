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
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
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
    int op_count_in_tx = 0;
    int current_tx_id = -1;
    
    char line[256];
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        
        int tx_id, start_tick;
        char op_type_str[16];
        int account_id, target_account = -1, amount = 0;
        
        // Parse line: TxID  StartTick  OpType  AccountID  [Amount]  [TargetAccount]
        int ret = sscanf(line, "T%d %d %s %d %d %d",
                        &tx_id, &start_tick, op_type_str, &account_id, &amount, &target_account);
        
        if (ret < 4) {
            fprintf(stderr, "Error: Invalid line in trace file: %s", line);
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
            op_count_in_tx = 0;
            (*transactions)[tx_count].tx_id = tx_id;
            (*transactions)[tx_count].start_tick = start_tick;
            (*transactions)[tx_count].num_ops = 0;
            (*transactions)[tx_count].status = TX_RUNNING;
        }
        
        // Parse operation type
        OpType op_type;
        if (strcmp(op_type_str, "DEPOSIT") == 0) {
            op_type = OP_DEPOSIT;
        } else if (strcmp(op_type_str, "WITHDRAW") == 0) {
            op_type = OP_WITHDRAW;
        } else if (strcmp(op_type_str, "TRANSFER") == 0) {
            op_type = OP_TRANSFER;
        } else if (strcmp(op_type_str, "BALANCE") == 0) {
            op_type = OP_BALANCE;
        } else {
            fprintf(stderr, "Error: Unknown operation type: %s\n", op_type_str);
            fclose(fp);
            free(*transactions);
            return -1;
        }
        
        // Add operation to current transaction
        int op_idx = (*transactions)[tx_count].num_ops;
        (*transactions)[tx_count].ops[op_idx].type = op_type;
        (*transactions)[tx_count].ops[op_idx].account_id = account_id;
        (*transactions)[tx_count].ops[op_idx].amount_centavos = amount;
        (*transactions)[tx_count].ops[op_idx].target_account = target_account;
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