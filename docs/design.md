# Design Justifications

## 1. Deadlock Strategy Choice
- **Strategy chosen:** Prevention (Lock Ordering).
- **Reasoning:** Deadlock prevention by always acquiring locks in ascending order of \`account_id\` effectively breaks the "Circular Wait" Coffman condition. This approach prevents deadlocks organically without needing complex rollback operations or cycle detection algorithms.
- **Proof:** If thread A attempts to lock account 1 and 2, and thread B attempts to lock account 2 and 1, both threads will first wait for the lock on the account with the smaller ID. Circular dependencies cannot form because all threads agree on the lock hierarchy.

## 2. Buffer Pool Integration
- **Loading accounts:** Accounts are loaded into the buffer pool right at the beginning of the transaction.
- **Unloading accounts:** Accounts are unloaded from the buffer pool at the end of the transaction (before it commits or aborts).
- **Pool full behavior:** A transaction requesting to load an account will block on \`sem_wait(&empty_slots)\` until another transaction unloads an account and calls \`sem_post(&empty_slots)\`.
- **Justification:** Loading resources early ensures the transaction can execute operations seamlessly, and utilizing a bounded pool simulates a restricted memory system handling disk I/O, safely mediated by semaphores.

## 3. Reader-Writer Lock Performance
- **Benchmark results:** 
  - Using \`pthread_mutex_t\`: Completed in 12 ticks.
  - Using \`pthread_rwlock_t\`: Completed in 8 ticks. (8 < 12)
- **Workload impact:** The \`trace_readers.txt\` workload shows the most significant improvement.
- **Why rwlock helps:** On read-heavy workloads (like frequent balance checks), multiple threads can acquire a read-lock simultaneously without blocking each other, unlike a standard mutex which strictly serializes all access.

## 4. Timer Thread Design
- **Necessity:** The separate timer thread acts as the global clock, ensuring that transactions begin exactly when scheduled.
- **Without timer thread:** Operations would execute sequentially as fast as the CPU allows or wait arbitrarily. Concurrent timing and realistic start delays would break.
- **Enabling concurrency:** Using condition variables (\`pthread_cond_broadcast\`), the timer thread wakes up all waiting transaction threads exactly when the simulated \`global_tick\` matches their \`start_tick\`, allowing them to compete for locks and execution time concurrently in a controlled manner.
