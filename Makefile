# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I. -Iinclude # Good warnings, debug symbols, and include path
LDFLAGS = -lcurl -lpthread -ldl -lncursesw -lssl -lcrypto # Libraries to link

# Plugin compilation flags
PLUGIN_CFLAGS = -fPIC -shared

# Main executables
TARGETS = 2bt a2

# Find all C files in the plugins directory
PLUGIN_SRCS = $(wildcard plugins/*.c)
# Convert the list of .c files to a list of .so files
PLUGIN_TARGETS = $(PLUGIN_SRCS:.c=.so)

# The 'all' target is the default one. It builds the main programs and all plugins.
all: $(TARGETS) $(PLUGIN_TARGETS)

# Rules to build the main executables
2bt: 2bt.c
	$(CC) $(CFLAGS) -o 2bt 2bt.c $(LDFLAGS)

# Source files for the a2 editor
A2_SRCS = a2.c timer.c

a2: $(A2_SRCS)
	$(CC) $(CFLAGS) -o a2 $(A2_SRCS) $(LDFLAGS)

# A pattern rule to build any .so plugin from its .c source file
# $@ is the target file (e.g., plugins/todo.so)
# $< is the first prerequisite (e.g., plugins/todo.c)
plugins/%.so: plugins/%.c
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -o $@ $<

# Target to clean up all built files
clean:
	rm -f $(TARGETS) $(PLUGIN_TARGETS) *.o
