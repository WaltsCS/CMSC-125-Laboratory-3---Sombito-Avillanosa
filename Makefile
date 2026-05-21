CC = gcc
CFLAGS = -pthread -O2 -Wall -Wextra -I./include
DEBUG_FLAGS = -g -fsanitize=thread -pthread -I./include

all:
	$(CC) $(CFLAGS) src/*.c -o bankdb

debug:
	$(CC) $(DEBUG_FLAGS) src/*.c -o bankdb

clean:
	rm -f bankdb

test: all
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_simple.txt --deadlock=prevention
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_readers.txt --deadlock=prevention
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_deadlock.txt --deadlock=prevention
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_abort.txt --deadlock=prevention
	./bankdb --accounts=tests/accounts.txt --trace=tests/trace_buffer.txt --deadlock=prevention
