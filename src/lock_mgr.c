// Deadlock prevention (lock ordering) or detection (wait-for graph + DFS)
#include "lock_mgr.h"
#include "bank.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>      // usleep

// Maximum number of concurrent transactions supported
#define MAX_TX 1024

// ---------------------------------------------------------------------------
// Thread-local: set to the tx_id of the transaction running in this thread,
// or -1 for non-transaction threads (main, timer).
// ---------------------------------------------------------------------------
__thread int current_tx_id = -1;

// ---------------------------------------------------------------------------
// Wait-for graph
//
// wait_for[i] == j  means  transaction i is currently waiting to acquire a
//                          lock held by transaction j.
// wait_for[i] == -1 means  transaction i is not waiting.
// ---------------------------------------------------------------------------
static int  wait_for[MAX_TX];          // wait-for graph edges
static bool tx_active[MAX_TX];         // is tx i registered?
static bool tx_should_abort[MAX_TX];   // deadlock victim flag for tx i
static pthread_mutex_t wfg_lock = PTHREAD_MUTEX_INITIALIZER;

// Per-account: which transaction holds the write lock (-1 if none).
// Protected by wfg_lock.
static int account_write_holder[MAX_ACCOUNTS];

// ---------------------------------------------------------------------------
// Internal: DFS cycle detection.
// Returns the id of the youngest (highest tx_id) transaction in any cycle
// reachable from start, or -1 if no cycle is found.
// Must be called with wfg_lock held.
// ---------------------------------------------------------------------------
static int dfs_find_youngest_in_cycle(int start) {
    // visited / on_stack arrays for DFS
    bool visited[MAX_TX];
    bool on_stack[MAX_TX];
    memset(visited,  0, sizeof(visited));
    memset(on_stack, 0, sizeof(on_stack));

    // Iterative DFS using an explicit stack of (node, edge_followed) pairs.
    // We use a simple recursive-style helper instead for clarity.
    // Stack depth is bounded by MAX_TX so no stack overflow risk.

    // Record the path so we can find all members of the cycle.
    int path[MAX_TX];
    int path_len = 0;

    // Recursive inner DFS (implemented as a loop for safety).
    // Returns the youngest tx_id in a detected cycle, -1 otherwise.
    int node = start;
    int youngest = -1;

    // Use an iterative DFS with explicit stack.
    // stack entries: node index
    int stack[MAX_TX];
    int stack_top = 0;
    int parent[MAX_TX];
    memset(parent, -1, sizeof(parent));

    stack[stack_top++] = node;

    while (stack_top > 0 && youngest == -1) {
        int cur = stack[--stack_top];

        if (visited[cur]) {
            // We may have reached an on-stack node -> cycle
            if (on_stack[cur]) {
                // Trace back to find all cycle members and pick youngest
                // cur is the re-visited on-stack node (cycle entry point)
                int member = cur;
                youngest = member;
                // Walk path[] to collect cycle members
                for (int i = 0; i < path_len; i++) {
                    if (path[i] == cur) {
                        // Everything from path[i] onward is in the cycle
                        for (int k = i; k < path_len; k++) {
                            if (tx_active[path[k]] && path[k] > youngest) {
                                youngest = path[k];
                            }
                        }
                        break;
                    }
                }
            }
            continue;
        }

        visited[cur]  = true;
        on_stack[cur] = true;
        path[path_len++] = cur;

        int next = wait_for[cur];
        if (next != -1 && tx_active[next]) {
            if (!visited[next]) {
                stack[stack_top++] = next;
            } else if (on_stack[next]) {
                // Cycle found — collect members
                youngest = next;
                for (int i = 0; i < path_len; i++) {
                    if (path[i] == next) {
                        for (int k = i; k < path_len; k++) {
                            if (tx_active[path[k]] && path[k] > youngest) {
                                youngest = path[k];
                            }
                        }
                        break;
                    }
                }
                // Also compare cur itself
                if (cur > youngest) youngest = cur;
            }
        } else {
            // Dead end: pop back out of on_stack
            on_stack[cur] = false;
            if (path_len > 0) path_len--;
        }
    }

    return youngest;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void lock_mgr_init(void) {
    pthread_mutex_lock(&wfg_lock);
    for (int i = 0; i < MAX_TX; i++) {
        wait_for[i]       = -1;
        tx_active[i]      = false;
        tx_should_abort[i]= false;
    }
    for (int i = 0; i < MAX_ACCOUNTS; i++) {
        account_write_holder[i] = -1;
    }
    pthread_mutex_unlock(&wfg_lock);
}

void init_tx_lock_state(int tx_id) {
    if (tx_id < 0 || tx_id >= MAX_TX) return;
    pthread_mutex_lock(&wfg_lock);
    wait_for[tx_id]        = -1;
    tx_active[tx_id]       = true;
    tx_should_abort[tx_id] = false;
    pthread_mutex_unlock(&wfg_lock);
}

void cleanup_tx_lock_state(int tx_id) {
    if (tx_id < 0 || tx_id >= MAX_TX) return;
    pthread_mutex_lock(&wfg_lock);
    wait_for[tx_id]        = -1;
    tx_active[tx_id]       = false;
    tx_should_abort[tx_id] = false;
    pthread_mutex_unlock(&wfg_lock);
}

// ---------------------------------------------------------------------------
// Write-lock wrapper (used by deposit, withdraw, transfer)
// ---------------------------------------------------------------------------
bool acquire_write_lock(Account* acc) {
    int tx_id = current_tx_id;

    while (1) {
        // Check if we have already been chosen as a deadlock victim
        if (tx_id >= 0 && tx_id < MAX_TX) {
            pthread_mutex_lock(&wfg_lock);
            bool should_abort = tx_should_abort[tx_id];
            pthread_mutex_unlock(&wfg_lock);
            if (should_abort) {
                return false;
            }
        }

        // Try non-blocking write lock
        int rc = pthread_rwlock_trywrlock(&acc->lock);
        if (rc == 0) {
            // Acquired — record that we hold this account's write lock
            if (tx_id >= 0 && tx_id < MAX_TX) {
                int idx = (int)(acc - bank.accounts);
                if (idx >= 0 && idx < MAX_ACCOUNTS) {
                    pthread_mutex_lock(&wfg_lock);
                    account_write_holder[idx] = tx_id;
                    wait_for[tx_id] = -1;   // no longer waiting
                    pthread_mutex_unlock(&wfg_lock);
                }
            }
            return true;
        }

        // Lock is held — only run deadlock detection if strategy == detection
        if (strcmp(deadlock_strategy, "detection") == 0 &&
            tx_id >= 0 && tx_id < MAX_TX) {

            int idx = (int)(acc - bank.accounts);
            if (idx >= 0 && idx < MAX_ACCOUNTS) {
                pthread_mutex_lock(&wfg_lock);

                // Record the wait-for edge
                int holder = account_write_holder[idx];
                if (holder >= 0 && holder < MAX_TX && holder != tx_id) {
                    wait_for[tx_id] = holder;

                    // Run cycle detection
                    int victim = dfs_find_youngest_in_cycle(tx_id);
                    if (victim >= 0) {
                        tx_should_abort[victim] = true;
                        if (victim == tx_id) {
                            wait_for[tx_id] = -1;
                            pthread_mutex_unlock(&wfg_lock);
                            return false;  // this transaction is the victim
                        }
                    }
                }

                pthread_mutex_unlock(&wfg_lock);
            }
        }

        usleep(1000);  // back off 1 ms before retrying
    }
}

void release_write_lock(Account* acc) {
    int tx_id = current_tx_id;
    int idx = (int)(acc - bank.accounts);

    if (idx >= 0 && idx < MAX_ACCOUNTS && tx_id >= 0 && tx_id < MAX_TX) {
        pthread_mutex_lock(&wfg_lock);
        if (account_write_holder[idx] == tx_id) {
            account_write_holder[idx] = -1;
        }
        pthread_mutex_unlock(&wfg_lock);
    }

    pthread_rwlock_unlock(&acc->lock);
}

// ---------------------------------------------------------------------------
// Read-lock wrapper (used by get_balance)
// For simplicity under detection mode we also use a try-loop; reads don't
// record a holder since multiple readers can hold simultaneously.
// ---------------------------------------------------------------------------
bool acquire_read_lock(Account* acc) {
    int tx_id = current_tx_id;

    while (1) {
        if (tx_id >= 0 && tx_id < MAX_TX) {
            pthread_mutex_lock(&wfg_lock);
            bool should_abort = tx_should_abort[tx_id];
            pthread_mutex_unlock(&wfg_lock);
            if (should_abort) return false;
        }

        int rc = pthread_rwlock_tryrdlock(&acc->lock);
        if (rc == 0) {
            return true;
        }

        usleep(1000);
    }
}

void release_read_lock(Account* acc) {
    pthread_rwlock_unlock(&acc->lock);
}