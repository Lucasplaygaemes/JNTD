#include <dirent.h>
#include <dlfcn.h>
#include "plugin.h"

#define MAX_PLUGINS 100

Plugin loaded_plugins[MAX_PLUGINS];
int plugin_count = 0;

void load_plugins() {
	DIR *dir = opendir("plugins");
	if (!dir) {
		perror("Pasta de plugins não econtrada.\n");
		return;
	}
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_type == DT_REG &&
		   (strstr(entry->d_name, ".so") || strstr(entry->d_name, ".dll"))) {

			char path[256];
			snprintf(path, sizeof(path), "plugin/%s", entry->d_name);

			void *handle = dlopen(path, RTLD_LAZY);
			if (!handle) {
				fprintf(stderr, "Erro ao carregar o plugin: %s\n", dlerror());
				continue;
			}
			Plugin *(*get_plugin)() = dlsym(handle, "get_plugin");
			if (!get_plugin) {
				fprintf(stderr, "Erro plugin invalido: %s\n", entry->d_name);
				dlclose(handle);
				continue;
			}
			Plugin *plugin = get_plugin();
			if (plugin) {
				loaded_plugins[plugin_count++] = *plugin;
				printf("Plugin carregado: %s\n", plugin->name);
			}
		}
	}
	closedir(dir);
}
void execute_plugin(const char* name, const char* args) {
    for (int i = 0; i < plugin_count; i++) {
        if (strcmp(loaded_plugins[i].name, name) == 0) {
            loaded_plugins[i].execute(args);
            return;
        }
    }
    printf("Plugin não encontrado: %s\n", name);
}	
