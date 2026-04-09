// Transaction and operation types
typedef enum {
    OP_DEPOSIT,
    OP_WITHDRAW,
    OP_TRANSFER,
    OP_BALANCE
} OpType;

typedef struct {
    OpType type;
    int account_id;
    int amount_centavos;
    int target_account;
} Operation;

typedef enum {
    TX_RUNNING,
    TX_COMMITTED,
    TX_ABORTED
} TxStatus;

typedef struct {
    int tx_id;
    Operation ops[256];
    int num_ops;
    int start_tick;
    pthread_t thread;
    int actual_start;
    int actual_end;
    int wait_ticks;
    TxStatus status;
} Transaction;