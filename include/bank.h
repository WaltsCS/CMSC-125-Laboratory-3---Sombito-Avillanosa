#ifndef BANK_H
#define BANK_H

#include <pthread.h>
#include <stdbool.h>

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

int get_balance(int account_id);
bool deposit(int account_id, int amount_centavos);
bool withdraw(int account_id, int amount_centavos);
bool transfer(int from_id, int to_id, int amount_centavos);

#endif // BANK_H