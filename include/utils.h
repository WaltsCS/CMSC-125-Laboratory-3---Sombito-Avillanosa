#ifndef UTILS_H
#define UTILS_H

#include "transaction.h"
#include "bank.h"

// File parsing
int parse_accounts_file(const char* filename);
int parse_trace_file(const char* filename, Transaction** transactions, int* num_transactions);

// Utility functions
void print_error(const char* message);
void print_info(const char* message);

#endif // UTILS_H