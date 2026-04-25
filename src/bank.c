#include "bank.h"
#include <stdlib.h>
#include <stdio.h>

Bank bank;

void bank_init(int num_accounts) {
    if (num_accounts > MAX_ACCOUNTS) {
        fprintf(stderr, "Error: num_accounts (%d) exceeds MAX_ACCOUNTS (%d)\n",
                num_accounts, MAX_ACCOUNTS);
        exit(1);
    }
    
    bank.num_accounts = num_accounts;
    
    // Initialize each account
    for (int i = 0; i < num_accounts; i++) {
        bank.accounts[i].account_id = -1;  // Mark as empty
        bank.accounts[i].balance_centavos = 0;
        
        // Initialize rwlock
        int ret = pthread_rwlock_init(&bank.accounts[i].lock, NULL);
        if (ret != 0) {
            fprintf(stderr, "pthread_rwlock_init failed for account %d\n", i);
            exit(1);
        }
    }
    
    // Initialize bank lock
    int ret = pthread_mutex_init(&bank.bank_lock, NULL);
    if (ret != 0) {
        fprintf(stderr, "pthread_mutex_init failed for bank_lock\n");
        exit(1);
    }
}

Account* get_account(int account_id) {
    // Linear search
    for (int i = 0; i < bank.num_accounts; i++) {
        if (bank.accounts[i].account_id == account_id) {
            return &bank.accounts[i];
        }
    }
    return NULL;
}