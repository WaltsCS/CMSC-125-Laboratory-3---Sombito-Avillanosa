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

int get_balance(int account_id) {
    Account* acc = get_account(account_id);

    if (acc == NULL) {
        fprintf(stderr, "Error: account %d not found\n", account_id);
        return -1;
    }

    pthread_rwlock_rdlock(&acc->lock);

    int balance = acc->balance_centavos;

    pthread_rwlock_unlock(&acc->lock);

    return balance;
}

bool deposit(int account_id, int amount_centavos) {
    if (amount_centavos < 0) {
        fprintf(stderr, "Error: cannot deposit negative amount %d\n",
                amount_centavos);
        return false;
    }

    Account* acc = get_account(account_id);

    if (acc == NULL) {
        fprintf(stderr, "Error: account %d not found\n", account_id);
        return false;
    }

    pthread_rwlock_wrlock(&acc->lock);

    acc->balance_centavos += amount_centavos;

    pthread_rwlock_unlock(&acc->lock);

    return true;
}

bool withdraw(int account_id, int amount_centavos) {
    if (amount_centavos < 0) {
        fprintf(stderr, "Error: cannot withdraw negative amount %d\n",
                amount_centavos);
        return false;
    }

    Account* acc = get_account(account_id);

    if (acc == NULL) {
        fprintf(stderr, "Error: account %d not found\n", account_id);
        return false;
    }

    pthread_rwlock_wrlock(&acc->lock);

    if (acc->balance_centavos < amount_centavos) {
        pthread_rwlock_unlock(&acc->lock);
        return false;
    }

    acc->balance_centavos -= amount_centavos;

    pthread_rwlock_unlock(&acc->lock);

    return true;
}

bool transfer(int from_id, int to_id, int amount_centavos) {
    if (amount_centavos < 0) {
        fprintf(stderr, "Error: cannot transfer negative amount %d\n",
                amount_centavos);
        return false;
    }

    if (from_id == to_id) {
        fprintf(stderr, "Error: cannot transfer from account %d to itself\n",
                from_id);
        return false;
    }

    Account* from_acc = get_account(from_id);
    Account* to_acc = get_account(to_id);

    if (from_acc == NULL) {
        fprintf(stderr, "Error: source account %d not found\n", from_id);
        return false;
    }

    if (to_acc == NULL) {
        fprintf(stderr, "Error: target account %d not found\n", to_id);
        return false;
    }


    // For deadlock prevention via lock ordering:
    // always lock the account with the smaller account_id first. 
    Account* first_acc;
    Account* second_acc;

    if (from_acc->account_id < to_acc->account_id) {
        first_acc = from_acc;
        second_acc = to_acc;
    } else {
        first_acc = to_acc;
        second_acc = from_acc;
    }

    pthread_rwlock_wrlock(&first_acc->lock);
    pthread_rwlock_wrlock(&second_acc->lock);

    if (from_acc->balance_centavos < amount_centavos) {
        pthread_rwlock_unlock(&second_acc->lock);
        pthread_rwlock_unlock(&first_acc->lock);
        return false;
    }

    from_acc->balance_centavos -= amount_centavos;
    to_acc->balance_centavos += amount_centavos;

    pthread_rwlock_unlock(&second_acc->lock);
    pthread_rwlock_unlock(&first_acc->lock);

    return true;
}