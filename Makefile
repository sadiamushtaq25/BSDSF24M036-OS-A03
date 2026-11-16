CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c11 -g -Iinclude
READLINE_CFLAGS := $(shell pkg-config --cflags readline 2>/dev/null)
READLINE_LIBS := $(shell pkg-config --libs readline 2>/dev/null)
ifneq ($(strip $(READLINE_LIBS)),)
CFLAGS += $(READLINE_CFLAGS) -DHAVE_READLINE
LDLIBS += $(READLINE_LIBS)
else
$(warning GNU Readline development files not found; building fallback input mode. Install libreadline-dev for Feature 4.)
endif

SRC := src/main.c src/shell.c
OBJ := obj/main.o obj/shell.o
TARGET := bin/myshell

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

obj/%.o: src/%.c include/shell.h
	@mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin
