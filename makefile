# MAKE FILE PARA O JTND
#
#
CC = gcc

#----- FLAGS -------#
#The flags are code when compiling a code#
# -Wall -Wextra: The common and extra warnings, good for cathing bugs.
# g inclue debugging information in the executable.
# -Iinclude tell to gcc look for the headers files in the 'include/'
CFLAGS = -Wall -Wextra -g -Iinclude

LDFLAGS =

# ldlibs are libraly files to link within the main aplication.
#
LDLIBS = -lcurl -ljson-c -pthread -ldl

TARGET = 2bt

CALC_TARGET = calc

PLUGIN_SCRS = $(wildcard plugins/plugin_*.c)

PLUGINS = $(patsubst
      plugins/plugin_%.c,plugins/%.so,$(PLUGIN_SRCS))

all = $(TARGET) $(CALC_TARGET) $(PLUGINS)

$(TARGET): 2bt.o
$(CC) $(LDFLAGS) -o $@ $< $(LSLIBS)
 @echo"Main application '$(TARGET)' built sucessfully."

$(CALC_TARGET): calc.o
$(CC) $(LDFLAGS) -o $@ $< -lm
 @echo"Calculator utility '$(CALC_TARGET)' built sucessfully."

plugins/%.so: plugins/plugin_%.c
$(CC) $(CFLAGS) -fPIC -shared -o $@ $<
 @echo"Plugin '$@' built sucessfully."

%.o: %.c
$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo"Cleaning up generated files..."
	rm -f$(TARGET) $(CALC_TARGET) 2bt.c calc.o plugins/*.so

.PHONY: all clean
