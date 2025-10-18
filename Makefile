# Makefile for JNTD

# --- Compiler Configuration ---
CC = gcc
CFLAGS = -g -Wall -Wextra -I. -I../include -I/usr/local/include
LDFLAGS = -lncursesw -lcurl -lpthread -ldl -lssl -lcrypto

# --- Main Target ---
TARGET = jntd

# --- Plugins ---
PLUGIN_CFLAGS = -fPIC -shared
PLUGIN_SRCS = $(wildcard plugins/*.c)
PLUGIN_TARGETS = $(PLUGIN_SRCS:.c=.so)

# Default target 'all' compiles the executable and plugins
all: $(TARGET) $(PLUGIN_TARGETS)

# --- Compilation Rules for 'jntd' ---
jntd: jntd.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# --- Compilation Rules for Plugins ---
plugins/%.so: plugins/%.c
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -o $@ $<

# --- Clean and Utility Targets ---
clean:
	rm -f $(TARGET) $(PLUGIN_TARGETS)

.PHONY: all clean
