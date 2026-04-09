CC = gcc
CFLAGS_RELEASE = -pthread -O2 -Wall -Wextra
CFLAGS_DEBUG = -g -fsanitize=thread -pthread

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TEST_DIR = tests

# Sources and objects
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = $(BIN_DIR)/bankdb

# Targets
all: CFLAGS = $(CFLAGS_RELEASE)
all: $(EXECUTABLE)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(EXECUTABLE)

$(BIN_DIR)/$(EXECUTABLE): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

test: debug
	@echo "=== Test 1: Simple Transactions ==="
	$(BIN_DIR)/$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_simple.txt --deadlock=prevention
	
	@echo "\n=== Test 2: Concurrent Readers ==="
	$(BIN_DIR)/$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_readers.txt --deadlock=prevention
	
	@echo "\n=== Test 3: Deadlock Scenario ==="
	$(BIN_DIR)/$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_deadlock.txt --deadlock=prevention
	
	@echo "\n=== Test 4: Insufficient Funds ==="
	$(BIN_DIR)/$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_abort.txt --deadlock=prevention
	
	@echo "\n=== Test 5: Buffer Pool Saturation ==="
	$(BIN_DIR)/$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_buffer.txt --deadlock=prevention

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all debug clean test