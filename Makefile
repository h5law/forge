CC      := cc
CFLAGS  := -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS := -Iinclude

TARGET := forge

SRC := \
	src/main.c \
	src/config.c

TEST_TARGET := test-config

TEST_SRC := \
	tests/config.c \
	src/config.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRC)

$(TEST_TARGET): $(TEST_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_SRC)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)
