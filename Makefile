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

FIXTURE_DIR := tests/fixtures

FIXTURE_CHILD := $(FIXTURE_DIR)/child/libforge-child.so
FIXTURE_PARENT := $(FIXTURE_DIR)/lib/libforge-parent.so
FIXTURE_RPATH := $(FIXTURE_DIR)/bin/forge-rpath
FIXTURE_RUNPATH := $(FIXTURE_DIR)/bin/forge-runpath

FIXTURES := \
	$(FIXTURE_CHILD) \
	$(FIXTURE_PARENT) \
	$(FIXTURE_RPATH) \
	$(FIXTURE_RUNPATH)

ROOTFS := rootfs

.PHONY: all clean test fixtures

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(SRC)

$(TEST_CONFIG_TARGET): $(TEST_CONFIG_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_CONFIG_SRC)

$(TEST_ALPINE_TARGET): $(TEST_ALPINE_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_ALPINE_SRC)

$(TEST_ELF_TARGET): $(TEST_ELF_SRC)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_ELF_SRC)

$(TEST_RESOLVER_TARGET): $(TEST_RESOLVER_SRC) $(FIXTURES)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $(TEST_RESOLVER_SRC)

$(FIXTURE_DIR)/child:
	mkdir -p $@

$(FIXTURE_DIR)/lib:
	mkdir -p $@

$(FIXTURE_DIR)/bin:
	mkdir -p $@

$(FIXTURE_CHILD): $(FIXTURE_DIR)/child tests/fixtures/child.c
	$(CC) -shared -fPIC -o $@ tests/fixtures/child.c

$(FIXTURE_PARENT): $(FIXTURE_DIR)/lib $(FIXTURE_CHILD) tests/fixtures/parent.c
	$(CC) -shared -fPIC \
		-Wl,-soname,libforge-parent.so \
		-L$(FIXTURE_DIR)/child \
		-Wl,-rpath-link,$(FIXTURE_DIR)/child \
		-o $@ tests/fixtures/parent.c \
		-lforge-child

$(FIXTURE_RPATH): $(FIXTURE_DIR)/bin $(FIXTURE_PARENT) tests/fixtures/main.c
	$(CC) \
		-Wl,--disable-new-dtags \
		-Wl,-rpath,'$$ORIGIN/../lib:$$ORIGIN/../child' \
		-L$(FIXTURE_DIR)/lib \
		-o $@ tests/fixtures/main.c \
		-lforge-parent

$(FIXTURE_RUNPATH): $(FIXTURE_DIR)/bin $(FIXTURE_PARENT) tests/fixtures/main.c
	$(CC) \
		-Wl,--enable-new-dtags \
		-Wl,-rpath,'$$ORIGIN/../lib:$$ORIGIN/../child' \
		-L$(FIXTURE_DIR)/lib \
		-o $@ tests/fixtures/main.c \
		-lforge-parent

fixtures: $(FIXTURES)

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
		$(FIXTURE_DIR)/bin \
		$(FIXTURE_DIR)/child \
		$(FIXTURE_DIR)/lib \
		$(ROOTFS)
