#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "plugin.h"

#define MAX_TODO_LINES 100
static char lines[MAX_TODO_LINES][1024];

void handle_add_todo();
void list_todo();
void remove_todo();
void edit_todo();
void check_todos();
void edit_with_vim();
time_t parse_date(const char *date_str);

void execute_todo(const char* args) {
    char command[256] = {0};

    if (args != NULL && strlen(args) > 0) {
        sscanf(args, "%255s", command);
    } else {
        // If no args, show help for this plugin
        printf("Comando TODO incompleto. Uso: todo <ação>\n");
        printf("Ações disponiveis: add, list, remove, edit, check, vim\n");
        return;
    }

    if (strcmp(command, "add") == 0) {
        handle_add_todo();
    } else if (strcmp(command, "list") == 0) {
        list_todo();
    } else if (strcmp(command, "remove") == 0) {
        remove_todo();
    } else if (strcmp(command, "edit") == 0) {
        edit_todo();
    } else if (strcmp(command, "check") == 0) {
        check_todos();
    } else if (strcmp(command, "vim") == 0) {
        edit_with_vim();
    } else {
        printf("Ação TODO desconhecida: '%s'. Use 'add', 'list', 'remove', 'edit', 'check', 'vim'.\n", command);
    }
}
Plugin* register_plugin() {
    static Plugin todo_plugin = {
        "todo",
        execute_todo
    };
    return &todo_plugin;
}
time_t parse_date(const char *date_str) {
    if (strcmp(date_str, "Sem prazo") == 0) {
        return -1;
    }
    struct tm tm = {0};
    if (sscanf(date_str, "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year) != 3) {
        return -1;
    }
    tm.tm_mon -= 1;
    tm.tm_year -= 1900;
    tm.tm_hour = 23; tm.tm_min = 59; tm.tm_sec = 59;
    return mktime(&tm);
}
void list_todo() {
    int count = 0;
    printf("--- Lista de Tarefas ---\n");
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        printf("Arquivo todo.txt nao encontrado ou vazio.\n");
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        printf("%d: %s", ++count, line);
    }
    fclose(todo_file);
    if (count == 0) {
        printf("Nenhum TODO encontrado.\n");
    }
    printf("------------------------\n");
}
void remove_todo() {
    list_todo();
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir todo.txt para remocao");
        return;
    }

    int count = 0;
    while (fgets(lines[count], sizeof(lines[0]), todo_file) != NULL && count < MAX_TODO_LINES) {
        count++;
    }
    fclose(todo_file);

    if (count == 0) {
        printf("Nenhum TODO para remover.\n");
        return;
    }

    char temp_input[256];
    printf("Digite o numero do TODO a ser removido (1-%d, ou 0 para cancelar): ", count);
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler entrada");
        return;
    }
    int index = atoi(temp_input);
    if (index <= 0 || index > count) {
        printf("Operacao cancelada ou numero invalido.\n");
        return;
    }

    FILE *temp_file = fopen("todo_temp.txt", "w");
    if (temp_file == NULL) {
        perror("Erro ao criar arquivo temporario");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (i != index - 1) {
            fprintf(temp_file, "%s", lines[i]);
        }
    }
    fclose(temp_file);

    if (remove("todo.txt") != 0 || rename("todo_temp.txt", "todo.txt") != 0) {
        perror("Erro ao atualizar o arquivo todo.txt");
        return;
    }
    printf("TODO numero %d removido com sucesso.\n", index);
}
void edit_todo() {
    list_todo();
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir todo.txt para edicao");
        return;
    }

    int count = 0;
    while (fgets(lines[count], sizeof(lines[0]), todo_file) != NULL && count < MAX_TODO_LINES) {
        count++;
    }
    fclose(todo_file);

    if (count == 0) {
        printf("Nenhum TODO para editar.\n");
        return;
    }

    char temp_input[256];
    printf("Digite o numero do TODO a ser editado (1-%d, ou 0 para cancelar): ", count);
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler entrada");
        return;
    }
    int index = atoi(temp_input);
    if (index <= 0 || index > count) {
        printf("Operacao cancelada ou numero invalido.\n");
        return;
    }

    char nova_tarefa[512];
    printf("Digite o novo texto da tarefa:\n");
    fflush(stdout);
    if (fgets(nova_tarefa, sizeof(nova_tarefa), stdin) == NULL) {
        perror("Erro ao ler nova tarefa");
        return;
    }
    nova_tarefa[strcspn(nova_tarefa, "\n")] = 0;

    FILE *temp_file = fopen("todo_temp.txt", "w");
    if (temp_file == NULL) {
        perror("Erro ao criar arquivo temporario");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (i == index - 1) {
            fprintf(temp_file, "%s\n", nova_tarefa);
        } else {
            fprintf(temp_file, "%s", lines[i]);
        }
    }
    fclose(temp_file);

    if (remove("todo.txt") != 0 || rename("todo_temp.txt", "todo.txt") != 0) {
        perror("Erro ao atualizar o arquivo todo.txt");
        return;
    }
    printf("TODO numero %d editado com sucesso.\n", index);
}

void handle_add_todo() {
    char tarefa[512];
    printf("Digite o assunto da tarefa: ");
    fflush(stdout);
    if (fgets(tarefa, sizeof(tarefa), stdin) == NULL) {
        perror("Erro ao ler o assunto da tarefa");
        return;
    }
    tarefa[strcspn(tarefa, "\n")] = 0; // Remove newline

    if (strlen(tarefa) == 0) {
        printf("Erro: Nenhuma tarefa fornecida. Operacao cancelada.\n");
        return;
    }

    FILE *todo_file = fopen("todo.txt", "a");
    if (todo_file == NULL) {
        perror("Erro ao abrir todo.txt");
        return;
    }
    fprintf(todo_file, "%s\n", tarefa);
    fclose(todo_file);
    printf("TODO '%s' salvo com sucesso.\n", tarefa);
}

void check_todos() {
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) return; // Silently fail if no file

    time_t now = time(NULL);
    printf("Verificando TODOs...\n");
    int has_overdue = 0;
    char line[1024];
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        char *prazo_start = strstr(line, "Prazo: ");
        if (prazo_start) {
            prazo_start += 7;
            char prazo[32] = {0};
            strncpy(prazo, prazo_start, sizeof(prazo) - 1);
            
            time_t prazo_time = parse_date(prazo);
            if (prazo_time != -1 && difftime(now, prazo_time) >= 0) {
                if (!has_overdue) {
                    printf("--- AVISO: Tarefas Vencidas ou Vencendo Hoje ---\n");
                    has_overdue = 1;
                }
                printf("  -> %s", line);
            }
        }
    }
    fclose(todo_file);
    if (!has_overdue) {
        printf("Nenhum TODO vencido encontrado.\n");
    }
}

void edit_with_vim() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        char *args[] = {"vim", "todo.txt", NULL};
        execvp("vim", args);
        perror("execvp"); // This only runs if execvp fails
        exit(1);
    }
    else {
        int status;
        waitpid(pid, &status, 0);
    }
}
