# Compiler and flags
CC = cc
CFLAGS = -Wall -Wextra -g -Iinclude # Good warnings, debug symbols, and include path
LDFLAGS = -lcurl -lpthread -ldl # Libraries to link

# Plugin compilation flags
PLUGIN_CFLAGS = -fPIC -shared

# Main executable
TARGET = 2bt

# Find all C files in the plugins directory
PLUGIN_SRCS = $(wildcard plugins/*.c)
# Convert the list of .c files to a list of .so files
PLUGIN_TARGETS = $(PLUGIN_SRCS:.c=.so)

# The 'all' target is the default one. It builds the main program and all plugins.
all: $(TARGET) $(PLUGIN_TARGETS)

# Rule to build the main executable
$(TARGET): 2bt.c
	$(CC) $(CFLAGS) -o $(TARGET) 2bt.c $(LDFLAGS)

# A pattern rule to build any .so plugin from its .c source file
# $@ is the target file (e.g., plugins/todo.so)
# $< is the first prerequisite (e.g., plugins/todo.c)
plugins/%.so: plugins/%.c
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -o $@ $<

# Target to clean up all built files
clean:
	rm -f $(TARGET) $(PLUGIN_TARGETS) *.o