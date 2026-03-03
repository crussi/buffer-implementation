CC          = gcc
WINCC       = x86_64-w64-mingw32-gcc

NCURSES_INC = -I/mingw64/include/ncurses
NCURSES_LIB = -L/mingw64/lib -lncursesw

CFLAGS      = -Wall -Wextra -Werror -Wpedantic -std=c99
LFLAGS      = -lncurses
WINLFLAGS   = $(NCURSES_LIB)
WINCFLAGS   = -Wall -Wextra -Werror -Wpedantic -std=c99 $(NCURSES_INC)
TARGET      = buffer-implementation
WIN_TARGET  = buffer-implementation.exe

UNITY_DIR   = unity/src
TEST_DIR    = tests
BUILD_DIR   = build

LIB_SOURCES = $(filter-out src/main.c, $(wildcard src/*.c))
ALL_SOURCES = $(wildcard src/*.c)

TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c)
TEST_TARGETS = $(patsubst $(TEST_DIR)/%.c, $(BUILD_DIR)/%, $(TEST_SOURCES))

TEST_CFLAGS  = -Wall -Wextra -std=c99 -g -Isrc -I$(UNITY_DIR) $(NCURSES_INC)
TEST_LFLAGS  = $(NCURSES_LIB)

.PHONY: all compile windows test clean

all: clean compile

compile:
	$(CC) $(CFLAGS) $(ALL_SOURCES) $(LFLAGS) -o $(TARGET)

windows:
	$(WINCC) $(WINCFLAGS) $(ALL_SOURCES) $(WINLFLAGS) -o $(WIN_TARGET)

test: $(TEST_TARGETS)
	@for t in $(TEST_TARGETS); do \
		echo "\nRunning $$t..."; \
		./$$t; \
	done

$(BUILD_DIR)/%: $(TEST_DIR)/%.c $(LIB_SOURCES) $(UNITY_DIR)/unity.c | $(BUILD_DIR)
	$(CC) $(TEST_CFLAGS) $^ -o $@ $(TEST_LFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f $(TARGET) $(WIN_TARGET)
	rm -rf $(BUILD_DIR)