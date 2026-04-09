#ifndef BANK_H
#define BANK_H

#include <pthread.h>

#define MAX_ACCOUNTS 100

typedef struct {
    int account_id;
    int balance_centavos;
    pthread_rwlock_t lock;
} Account;

typedef struct {
    Account accounts[MAX_ACCOUNTS];
    int num_accounts;
    pthread_mutex_t bank_lock;
} Bank;

// Global bank instance
extern Bank bank;

// Function declarations
void bank_init(int num_accounts);
Account* get_account(int account_id);

#endif // BANK_H