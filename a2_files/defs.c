#include "defs.h"

// Variável global para o gerenciador de janelas
GerenciadorWorkspaces gerenciador_workspaces;
char executable_dir[PATH_MAX] = {0};
char* global_yank_register = NULL;