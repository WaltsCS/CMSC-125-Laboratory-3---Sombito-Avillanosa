# Concurrent Banking System Design Documentation

## 1. Deadlock Strategy Choice

### Chosen Strategy: Deadlock Prevention via Lock Ordering

For this implementation, we chose **deadlock prevention using lock ordering**. This strategy is simpler, easier to verify, and less error-prone compared to deadlock detection using a wait-for graph.

The main operation where deadlock can occur is `TRANSFER`, because it needs to acquire write locks on two accounts at the same time. For example, if one transaction transfers from account 10 to account 20 while another transaction transfers from account 20 to account 10, both transactions could acquire one lock and wait forever for the other.

To prevent this, the system always acquires account locks in ascending account ID order, regardless of the transfer direction.

Example:

```txt
T1: TRANSFER 10 -> 20
T2: TRANSFER 20 -> 10
```

Both transactions acquire locks in this order:

```txt
lock account 10 first
lock account 20 second
```

Because all transactions follow the same global ordering rule, circular waiting cannot occur.

### Coffman Condition Broken

Deadlock requires four Coffman conditions:

1. Mutual exclusion
2. Hold and wait
3. No preemption
4. Circular wait

Our implementation breaks the circular wait condition. Since all account locks are acquired in increasing account ID order, there can be no cycle where one transaction waits for a lower-numbered lock while holding a higher-numbered lock.

### Implementation Location

The lock-ordering logic is implemented inside `transfer()` in `src/bank.c`.

The function first identifies the account with the smaller account ID, locks it first, then locks the account with the larger account ID. After the transfer is completed or aborted due to insufficient funds, the locks are released in reverse order.

---

## 2. Buffer Pool Integration

### Buffer Pool Design

The system includes a bounded buffer pool with a fixed size:

```c
#define BUFFER_POOL_SIZE 5
```

The buffer pool is implemented using:
- `sem_t empty_slots`
- `sem_t full_slots`
- `pthread_mutex_t pool_lock`

The semaphores control how many slots are available or occupied, while the mutex protects the internal buffer pool structure.

### Load and Unload Policy

Our current buffer pool policy is:

```
Load all accounts needed by a transaction before executing its operations, then unload them after the transaction finishes.
```

For each operation:
- `DEPOSIT`, `WITHDRAW`, and `BALANCE` load the primary account.
- `TRANSFER` loads both the source account and the target account.

At the end of the transaction, all loaded accounts are unloaded from the buffer pool.

This follows the Week 3 implementation plan, which specifies loading accounts before executing operations and unloading them at the end.

### What Happens If the Pool Is Full?

If the buffer pool is full, `load_account()` blocks on:

```c
sem_wait(&p->empty_slots);
```

This means a transaction must wait until another transaction unloads an account and frees a slot.

The implementation also tracks blocked operations by checking the semaphore value before waiting. If no empty slots are available, the `blocked_operations` counter is incremented.

### Performance and Correctness Justification

This policy is simple and easy to reason about. Since accounts are loaded before transaction execution and unloaded afterward, each transaction clearly marks which accounts it needs during its lifetime.

The limitation is that if a single transaction needs more accounts than the buffer pool size, it can block while trying to load all of them. A future improvement would be to load and unload accounts per operation or implement an eviction policy such as LRU.

For the current required trace files, the policy works correctly and produces buffer pool statistics such as total loads, total unloads, peak usage, and blocked operations.

---

## 3. Reader-Writer Lock Performance

### Why Reader-Writer Locks Are Used

Each account has its own `pthread_rwlock_t`.

This allows the system to distinguish between:
- read-only operations
- write operations

The `BALANCE` operation uses a read lock:

```c
pthread_rwlock_rdlock(...)
```
The `DEPOSIT`, `WITHDRAW`, and `TRANSFER` operations use write locks:

```c
pthread_rwlock_wrlock(...)
```

### Why This Helps

A normal mutex would allow only one thread to access an account at a time, even if all threads are only reading. This is too restrictive for read-heavy workloads.

Reader-writer locks improve concurrency because:
- multiple balance inquiries can read the same account at the same time
- writers still get exclusive access when modifying balances

### Read-Heavy Workload Result

The clearest read-heavy test is `trace_readers.txt`, where multiple transactions read the same account at the same start tick.

Observed result:

```txt
Parsed 4 transactions from trace file

Tick 0: T1 started
T1: Account 10 balance = PHP 500.00
Tick 0: T1 committed
Tick 0: T2 started
T2: Account 10 balance = PHP 500.00
Tick 0: T2 committed
Tick 0: T3 started
T3: Account 10 balance = PHP 500.00
Tick 0: T3 committed
Tick 0: T4 started
T4: Account 10 balance = PHP 500.00
Tick 0: T4 committed
```
Metrics:

```
Total transactions: 4
Final tick: 1
Average wait time: 0.0 ticks
Throughput: 4 transactions / 1 ticks = 4.00 tx/tick
Balance consistency check: PASSED

```
This shows that all reader transactions completed successfully and safely without modifying the balance.

### Mutex vs Reader-Writer Lock Comparison

A full benchmark comparison between `pthread_mutex_t` and `pthread_rwlock_t` is not yet included in the codebase. However, conceptually, reader-writer locks should provide the biggest improvement on `trace_readers.txt`, because all operations are read-only and can safely run concurrently.

For workloads dominated by writes, the performance benefit is smaller because write locks still require exclusive access.

---

## 4. Timer Thread Design

### Why a Separate Timer Thread Is Necessary

The system uses a separate timer thread to simulate time progression. This timer thread increments the global simulation clock and wakes transaction threads waiting for their scheduled start time.

The timer system uses:
- `global_tick`
- `tick_lock`
- `tick_changed`
- `simulation_running`

Transactions call `wait_until_tick(start_tick)` before executing. This allows transactions with the same start tick to become runnable at the same simulated time.

### Why Not Process Operations Sequentially?

If the system simply processed operations sequentially, then there would be no real concurrency. Transactions would run one after another, which would not properly test:
- race conditions
- reader-writer locks
- transfer lock ordering
- bounded buffer pool blocking
- thread scheduling behavior

Sequential processing would hide the concurrency problems that this lab is designed to demonstrate.

### How the Timer Enables Concurrency Testing

The timer thread uses a condition variable to wake transactions when time advances. Multiple transactions can be waiting for the same tick, and when that tick is reached, they can all proceed concurrently.

This allows test cases such as:

```txt
T1 0 BALANCE 10
T2 0 BALANCE 10
T3 0 BALANCE 10
T4 0 BALANCE 10
```

to start at the same simulation tick and test concurrent account access.

---

## 5. Test Results Summary

### Test: `trace_simple.txt`

Result:

```txt
T1: DEPOSIT account 10 amount PHP 50.00
T1: WITHDRAW account 10 amount PHP 20.00
T1: Account 10 balance = PHP 530.00
Tick 0: T1 committed

Initial total:  PHP 1720.00
Expected delta: PHP 30.00
Expected final: PHP 1750.00
Actual final:   PHP 1750.00
Balance consistency check: PASSED
```
This confirms that deposit, withdraw, balance inquiry, and adjusted balance consistency checking work correctly.

### Test: `trace_readers.txt`

Result:
```txt
Total transactions: 4
Average wait time: 0.0 ticks
Throughput: 4 transactions / 1 ticks = 4.00 tx/tick
Balance consistency check: PASSED
```
This confirms that concurrent reader transactions complete safely.

### Test: `trace_abort.txt`

Result:
```txt
T1: WITHDRAW account 10 amount PHP 800.00
T1 aborted: insufficient funds or invalid withdrawal

Balance consistency check: PASSED
```
This confirms that insufficient funds correctly aborts the transaction and does not modify the account balance.

### Test: trace_deadlock.txt

Result:
```txt
T1: TRANSFER from account 10 to account 20 amount PHP 50.00
Tick 0: T1 committed
T2: TRANSFER from account 20 to account 10 amount PHP 30.00
Tick 0: T2 committed

Balance consistency check: PASSED
```
This confirms that opposite-direction transfers complete successfully under the prevention strategy.

### Test: trace_buffer.txt

Result:
```txt
Total transactions: 6
Total loads: 6
Total unloads: 6
Expected delta: PHP 60.00
Actual final:   PHP 1780.00
Balance consistency check: PASSED
```
This confirms that the buffer pool load/unload integration is active and that balance updates remain consistent.

## 6. Known Limitations

1. The current implementation uses deadlock prevention only. The `--deadlock=detection` option is accepted by the CLI, but wait-for graph deadlock detection is not implemented.
2. The buffer pool currently loads all accounts needed by a transaction before executing operations and unloads them after the transaction finishes. This is simple, but it can block if one transaction needs more accounts than the buffer pool size.
3. The current logs show successful deadlock prevention behavior, but they do not yet explicitly print a message such as `[DEADLOCK PREVENTED] Lock ordering applied`.
4. Full transaction-level atomicity across multiple operations is not fully implemented. The system currently enforces account-level isolation for each banking operation using reader-writer locks.
5. The reader-writer lock vs mutex benchmark is explained conceptually, but a separate measured mutex-based implementation is not yet included.