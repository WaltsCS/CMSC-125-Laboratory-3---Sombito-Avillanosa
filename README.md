# CMSC 125 Lab 3: Concurrent Banking System

## Team Members
- [Team Member 1 Name]
- [Team Member 2 Name]

## Project Overview
A multi-threaded banking system using POSIX threads, reader-writer locks, semaphores, and deadlock prevention. Implements concurrent transaction processing with proper synchronization and performance monitoring.

## Building

### Compile Release Version
```bash
make all
```

## Implementation Roadmap

### Week 1: Foundation & Core Infrastructure
- Establish project structure, data models, and Makefile.
- Implement timer thread and CLI argument parsing.
- Develop file I/O utilities and account initialization.

### Week 2: Transaction Execution & Reader-Writer Locks
- Implement multi-threaded transaction execution.
- Integrate per-account reader-writer locks.
- Develop lock ordering strategy for deadlock prevention.

### Week 3: Buffer Pool, Metrics & Performance
- Implement a bounded buffer pool using semaphores.
- Integrate comprehensive performance metrics and transaction tracking.
- Conduct performance optimizations and balance conservation checks.

### Week 4: Final Testing, Documentation & Defense Prep
- Perform rigorous testing with ThreadSanitizer.
- Finalize documentation and design reports.
- Prepare technical walkthrough and performance demonstrations for defense.