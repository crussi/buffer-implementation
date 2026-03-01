CC          = gcc
WINCC       = x86_64-w64-mingw32-gcc

CFLAGS      = -Wall -Wextra -Werror -Wpedantic -std=c99
LFLAGS      = -lncurses
WINLFLAGS   = -L/mingw64/lib -lncursesw
WINCFLAGS   = -Wall -Wextra -Werror -Wpedantic -std=c99 -I/mingw64/include/ncurses
TARGET      = buffer-implementation
WIN_TARGET  = buffer-implementation.exe

UNITY_DIR   = unity/src
TEST_DIR    = tests
BUILD_DIR   = build

# All sources except main.c — tests supply their own entry point
LIB_SOURCES = $(filter-out src/main.c, $(wildcard src/*.c))
ALL_SOURCES = $(wildcard src/*.c)

TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/%, $(TEST_SOURCES))

TEST_CFLAGS  = -Wall -Wextra -std=c99 -g -Isrc -I$(UNITY_DIR)

.PHONY: all compile windows test clean

all: clean compile

compile:
	$(CC) $(CFLAGS) $(ALL_SOURCES) $(LFLAGS) -o $(TARGET)

windows:
	$(WINCC) $(WINCFLAGS) $(ALL_SOURCES) $(WINLFLAGS) -o $(WIN_TARGET)

# Build and run every test binary
test: $(TEST_TARGETS)
	@for t in $(TEST_TARGETS); do \
		echo "\nRunning $$t..."; \
		./$$t; \
	done

# Pattern rule: tests/test_foo.c -> build/test_foo
$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(LIB_SOURCES) $(UNITY_DIR)/unity.c | $(BUILD_DIR)
	$(CC) $(TEST_CFLAGS) $^ -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET) $(WIN_TARGET)
	rm -rf $(BUILD_DIR)