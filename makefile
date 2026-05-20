# Compiler and flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -pedantic-errors

.PHONY: all clean

# Default target
all: clean matrix signal

# Clean generated files
clean:
	@rm -f matrix signal

# Compile matrix.c
matrix: clean
	@$(CC) $(CFLAGS) matrix.c -o matrix

# Compile signal.c
signal: clean
	@$(CC) $(CFLAGS) signal.c -o signal

