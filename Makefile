CC = gcc
CFLAGS_RELEASE = -pthread -O2 -Wall -Wextra
CFLAGS_DEBUG = -g -fsanitize=thread -pthread

# Directories
SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
TEST_DIR = tests

# Sources and objects
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SOURCES))
EXECUTABLE = bankdb

# Targets
all: CFLAGS = $(CFLAGS_RELEASE)
all: $(EXECUTABLE)

debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

test: debug
	@echo "=== Test 1: Simple Transactions ==="
	./$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_simple.txt --deadlock=prevention
	
	@echo "\n=== Test 2: Concurrent Readers ==="
	./$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_readers.txt --deadlock=prevention
	
	@echo "\n=== Test 3: Deadlock Scenario ==="
	./$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_deadlock.txt --deadlock=prevention
	
	@echo "\n=== Test 4: Insufficient Funds ==="
	./$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_abort.txt --deadlock=prevention
	
	@echo "\n=== Test 5: Buffer Pool Saturation ==="
	./$(EXECUTABLE) --accounts=$(TEST_DIR)/accounts.txt --trace=$(TEST_DIR)/trace_buffer.txt --deadlock=prevention

clean:
	rm -rf $(OBJ_DIR) $(EXECUTABLE) bin

.PHONY: all debug clean test