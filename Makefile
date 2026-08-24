CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wno-unused-parameter -pedantic -g -Isrc
SRCS = $(wildcard src/*.c)
OBJS = $(SRCS:.c=.o)
TARGET = trco

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f src/*.o $(TARGET)

test: $(TARGET)
	bash tests/run_tests.sh
