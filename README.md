# CMSC 125 Lab 3: Concurrent Banking System

## Team Members
- Sombito, Chakinzo N.
- Avillanosa, Walton Karl

## Compilation
### Normal Build
```bash
make all
```

### Debug Build (with ThreadSanitizer)
```bash
make debug
```

### Clean Build Files
```bash
make clean
```

## Usage
Run the `bankdb` executable with the following options:

```bash
./bankdb [OPTIONS]
```

### Command-Line Options
- `--accounts=<file>`: Path to the initial accounts file.
- `--trace=<file>`: Path to the transaction trace file to execute.
- `--deadlock=<strategy>`: Deadlock handling strategy (`prevention` is implemented via lock ordering).
- `--tick-ms=<ms>`: Duration of a single global tick in milliseconds (default: 100ms).
- `--verbose`: Enable detailed logging of transactions and buffer pool operations.

### Example
```bash
./bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention --verbose
```

## Test Cases
The system includes 5 standard test cases to verify different synchronization mechanisms:
1. **Simple Trace (`trace_simple.txt`)**: Verifies basic transaction execution and balance consistency.
2. **Reader-Writer (`trace_readers.txt`)**: Demonstrates concurrent read performance using `pthread_rwlock_t`.
3. **Deadlock (`trace_deadlock.txt`)**: Proves deadlock prevention using lock ordering on concurrent opposite transfers.
4. **Abort (`trace_abort.txt`)**: Tests transaction atomicity and rollback on insufficient funds.
5. **Buffer (`trace_buffer.txt`)**: Verifies buffer pool saturation, blocking, and recovery using semaphores.

Run all tests using:
```bash
make test
```

## Implemented Features
- **Multi-threaded Execution**: Transactions run in parallel using POSIX threads.
- **Reader-Writer Locks**: Per-account locks for concurrent reads and exclusive writes.
- **Deadlock Prevention**: Deterministic lock ordering (ascending account IDs) to break circular wait.
- **Timer Thread**: Global tick management with condition variables for synchronized scheduling.
- **Bounded Buffer Pool**: Limits concurrent account access using semaphores to manage a fixed number of slots.
- **Performance Metrics**: Comprehensive reporting of transaction status, throughput, and buffer pool usage.
- **ThreadSanitizer Clean**: Zero warnings or data races under ThreadSanitizer analysis.
- **Data Integrity**: Money conservation check ensures initial and final bank totals match exactly.

## Known Limitations
- Buffer pool size is currently fixed at 5 slots via `BUFFER_POOL_SIZE` macro.
