# MAKEFILE FOR THE JNTD PROJECT

# Compiler to use
CC = gcc

# --- FLAGS ---
# -Wall -Wextra: Enable common and extra warnings, good for catching bugs.
# -g: Include debugging information in the executable.
# -Iinclude: Tell gcc to look for header files in the 'include/' directory.
CFLAGS = -Wall -Wextra -g -Iinclude

# Flags for the linker (can be left empty for now)
LDFLAGS =

# Libraries to link against for the main application.
# -lcurl: For libcurl (networking)
# -ljson-c: For the JSON parsing library
# -pthread: For POSIX threads (used for timers)
# -ldl: For dynamically loading plugins (.so files)
LDLIBS = -lcurl -pthread -ldl

# --- TARGETS ---

# The main application executable
TARGET = 2bt

# The calculator utility executable
CALC_TARGET = calc

# Automatically find all plugin source files
PLUGIN_SRCS = $(wildcard plugins/plugin_*.c)

# Automatically generate the target .so names from the .c source names
PLUGINS = $(patsubst plugins/plugin_%.c,plugins/%.so,$(PLUGIN_SRCS))


# --- BUILD RULES ---

# The 'all' rule is the default. It builds everything.
all: $(TARGET) $(CALC_TARGET) $(PLUGINS)

# Rule to link the main application executable
$(TARGET): 2bt.o
	$(CC) $(LDFLAGS) -o $@ $< $(LDLIBS)
	@echo "Main application '$(TARGET)' built successfully."

# Rule to link the calculator utility
$(CALC_TARGET): calc.o
	$(CC) $(LDFLAGS) -o $@ $< -lm
	@echo "Calculator utility '$(CALC_TARGET)' built successfully."

# Pattern rule to build any plugin .so from its corresponding .c file
plugins/%.so: plugins/plugin_%.c
	$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
	@echo "Plugin '$@' built successfully."

# Generic pattern rule to compile any .c file into a .o object file
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# The 'clean' rule removes all generated files
clean:
	@echo "Cleaning up generated files..."
	rm -f $(TARGET) $(CALC_TARGET) 2bt.o calc.o plugins/*.so

# .PHONY declares targets that are not actual files
.PHONY: all clean
