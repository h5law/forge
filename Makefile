CC := cc

CFLAGS := -std=c11 -D_GNU_SOURCE -Wall -Wextra -Wpedantic -Werror -O2 -Iinclude

FORGE := forge

TEST_CONFIG := tests/test-config
TEST_ALPINE := tests/test-alpine
TEST_ELF := tests/test-elf
TEST_RESOLVER := tests/test-resolver

FIXTURE_BIN_DIR := tests/fixtures/bin
FIXTURE_CHILD_DIR := tests/fixtures/child
FIXTURE_LIB_DIR := tests/fixtures/lib
FIXTURE_MISSING_DIR := tests/fixtures/missing

FIXTURE_CHILD := $(FIXTURE_CHILD_DIR)/libforge-child.so
FIXTURE_PARENT := $(FIXTURE_LIB_DIR)/libforge-parent.so

FIXTURE_RPATH := $(FIXTURE_BIN_DIR)/forge-rpath
FIXTURE_RUNPATH := $(FIXTURE_BIN_DIR)/forge-runpath
FIXTURE_MISSING_LIB := $(FIXTURE_MISSING_DIR)/libforge-missing.so
FIXTURE_MISSING := $(FIXTURE_BIN_DIR)/forge-missing

FIXTURES := \
	$(FIXTURE_RPATH) \
	$(FIXTURE_RUNPATH) \
	$(FIXTURE_MISSING)

TESTS := \
	$(TEST_CONFIG) \
	$(TEST_ALPINE) \
	$(TEST_ELF) \
	$(TEST_RESOLVER)


.PHONY: all
all: $(FORGE)


$(FORGE): \
	src/main.c \
	src/config.c \
	src/alpine.c \
	src/elf.c \
	src/resolver.c \
	include/config.h \
	include/alpine.h \
	include/elf_parser.h \
	include/resolver.h
	$(CC) $(CFLAGS) -o $@ \
		src/main.c \
		src/config.c \
		src/alpine.c \
		src/elf.c \
		src/resolver.c


.PHONY: test
test: $(TESTS)
	./$(TEST_CONFIG)
	./$(TEST_ALPINE)
	./$(TEST_ELF)
	./$(TEST_RESOLVER)


$(TEST_CONFIG): tests/config.c tests/utils.c src/config.c include/config.h
	$(CC) $(CFLAGS) -o $@ \
		tests/config.c \
		tests/utils.c \
		src/config.c


$(TEST_ALPINE): tests/alpine.c tests/utils.c src/alpine.c include/alpine.h
	$(CC) $(CFLAGS) -o $@ \
		tests/alpine.c \
		tests/utils.c \
		src/alpine.c


$(TEST_ELF): tests/elf.c tests/utils.c src/elf.c include/elf_parser.h
	$(CC) $(CFLAGS) -o $@ \
		tests/elf.c \
		tests/utils.c \
		src/elf.c


$(TEST_RESOLVER): \
	tests/resolver.c \
	tests/utils.c \
	src/resolver.c \
	src/elf.c \
	include/resolver.h \
	include/elf_parser.h \
	$(FIXTURES)
	$(CC) $(CFLAGS) -o $@ \
		tests/resolver.c \
		tests/utils.c \
		src/resolver.c \
		src/elf.c


$(FIXTURE_CHILD): tests/fixtures/child.c | $(FIXTURE_CHILD_DIR)
	$(CC) \
		-shared \
		-fPIC \
		-o $@ $<


$(FIXTURE_PARENT): tests/fixtures/parent.c $(FIXTURE_CHILD) | $(FIXTURE_LIB_DIR)
	$(CC) \
		-shared \
		-fPIC \
		-Wl,-soname,libforge-parent.so \
		-L$(FIXTURE_CHILD_DIR) \
		-Wl,-rpath-link,$(FIXTURE_CHILD_DIR) \
		-o $@ tests/fixtures/parent.c \
		-lforge-child


$(FIXTURE_RPATH): tests/fixtures/main.c $(FIXTURE_PARENT) | $(FIXTURE_BIN_DIR)
	$(CC) \
		-Wl,--disable-new-dtags \
		-Wl,-rpath,'$$ORIGIN/../lib:$$ORIGIN/../child' \
		-L$(FIXTURE_LIB_DIR) \
		-o $@ tests/fixtures/main.c \
		-lforge-parent


$(FIXTURE_RUNPATH): tests/fixtures/main.c $(FIXTURE_PARENT) | $(FIXTURE_BIN_DIR)
	$(CC) \
		-Wl,--enable-new-dtags \
		-Wl,-rpath,'$$ORIGIN/../lib:$$ORIGIN/../child' \
		-L$(FIXTURE_LIB_DIR) \
		-o $@ tests/fixtures/main.c \
		-lforge-parent


$(FIXTURE_MISSING_LIB): tests/fixtures/child.c | $(FIXTURE_MISSING_DIR)
	$(CC) \
		-shared \
		-fPIC \
		-Wl,-soname,libforge-missing.so \
		-o $@ $<


$(FIXTURE_MISSING): tests/fixtures/missing_main.c $(FIXTURE_MISSING_LIB) | $(FIXTURE_BIN_DIR)
	$(CC) \
		-Wl,--disable-new-dtags \
		-Wl,--no-as-needed \
		-Wl,-rpath,'$$ORIGIN/../missing' \
		-L$(FIXTURE_MISSING_DIR) \
		-o $@ tests/fixtures/missing_main.c \
		-lforge-missing
	rm -f $(FIXTURE_MISSING_LIB)


$(FIXTURE_CHILD_DIR):
	mkdir -p $@


$(FIXTURE_LIB_DIR):
	mkdir -p $@


$(FIXTURE_BIN_DIR):
	mkdir -p $@


$(FIXTURE_MISSING_DIR):
	mkdir -p $@


.PHONY: clean
clean:
	rm -dfr \
		$(FORGE) \
		$(TEST_CONFIG) \
		$(TEST_ALPINE) \
		$(TEST_ELF) \
		$(TEST_RESOLVER) \
		$(FIXTURE_BIN_DIR) \
		$(FIXTURE_CHILD_DIR) \
		$(FIXTURE_LIB_DIR) \
		$(FIXTURE_MISSING_DIR) \
		rootfs
