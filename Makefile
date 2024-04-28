CC=gcc
CFLAGS=-Wall -Wextra
TARGET=coop

SRC_FILES=$(wildcard *.c)
OBJ_FILES=$(SRC_FILES:.c=.o)

$(TARGET): $(OBJ_FILES)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c %.h
	$(CC) $(CFLAGS) -c -o $@ $<
