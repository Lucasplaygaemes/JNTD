#ifndef PLUGIN_H
#define PLUGIN_H

typedef void (*plugin_func)(const char*);

typedef struct {
	const char *name;
	plugin_func execute;
} Plugin;

#endif
