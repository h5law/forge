CC       := cc
CFLAGS   := -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Werror -O2
CPPFLAGS := -Iinclude

TARGET := forge

SRC := \
	src/main.c \
	src/config.c \
	src/alpine.c \
	src/elf.c \
	src/resolver.c

TEST_CONFIG_TARGET := tests/test-config
TEST_CONFIG_SRC := \
	tests/config.c \
	tests/utils.c \
	src/config.c

TEST_ALPINE_TARGET := tests/test-alpine
TEST_ALPINE_SRC := \
	tests/alpine.c \
	tests/utils.c \
	src/alpine.c

TEST_ELF_TARGET := tests/test-elf
TEST_ELF_SRC := \
	tests/elf.c \
	tests/utils.c \
	src/elf.c

TEST_RESOLVER_TARGET := tests/test-resolver
TEST_RESOLVER_SRC := \
	tests/resolver.c \
	tests/utils.c \
	src/resolver.c \
	src/elf.c

ROOTFS := rootfs

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRC)

$(TEST_CONFIG_TARGET): $(TEST_CONFIG_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_CONFIG_SRC)

$(TEST_ALPINE_TARGET): $(TEST_ALPINE_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_ALPINE_SRC)

$(TEST_ELF_TARGET): $(TEST_ELF_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_ELF_SRC)

$(TEST_RESOLVER_TARGET): $(TEST_RESOLVER_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_RESOLVER_SRC)

test: \
	$(TEST_CONFIG_TARGET) \
	$(TEST_ALPINE_TARGET) \
	$(TEST_ELF_TARGET) \
	$(TEST_RESOLVER_TARGET)
	./$(TEST_CONFIG_TARGET)
	./$(TEST_ALPINE_TARGET)
	./$(TEST_ELF_TARGET)
	./$(TEST_RESOLVER_TARGET)

clean:
	rm -dfr $(TARGET) \
		$(TEST_CONFIG_TARGET) \
		$(TEST_ALPINE_TARGET) \
		$(TEST_ELF_TARGET) \
		$(TEST_RESOLVER_TARGET) \
		$(ROOTFS)
