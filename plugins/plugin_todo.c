#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <ctype.h>
#include "plugin.h"

#define MAX_TODO_LINES 100
static char lines[MAX_TODO_LINES][1024];
static int count;

// Forward declarations
void check_todos();
void handle_add_todo(const char *input);
void list_todo();
void remove_todo(const char *args);
void edit_todo(const char *args);
void edit_with_vim();
time_t parse_date(const char *date_str);

void execute_todo(const char* args) {
    char command[256] = {0};
    const char* sub_args = "";

    if (args == NULL || strlen(args) == 0) {
        printf("Comando TODO incompleto. Uso: todo <ação>\n");
        printf("Ações disponiveis: add, list, remove, edit, check, vim\n");
        return;
    }

    const char* first_space = strchr(args, ' ');
    if (first_space) {
        size_t cmd_len = first_space - args;
        if (cmd_len < sizeof(command)) {
            strncpy(command, args, cmd_len);
            command[cmd_len] = '\0';
        } else {
            strncpy(command, args, sizeof(command) - 1);
            command[sizeof(command) - 1] = '\0';
        }
        sub_args = first_space + 1;
        while (*sub_args && isspace((unsigned char)*sub_args)) {
            sub_args++;
        }
    } else {
        strncpy(command, args, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
    }

    if (strcmp(command, "add") == 0) {
        handle_add_todo(sub_args);
    } else if (strcmp(command, "list") == 0) {
        list_todo();
    } else if (strcmp(command, "remove") == 0) {
        remove_todo(sub_args);
    } else if (strcmp(command, "edit") == 0) {
        edit_todo(sub_args);
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

void check_todos() {
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        if (access("todo.txt", F_OK) == -1) {
            printf("Nenhum TODO encontrado.\n");
        } else {
            perror("Erro ao abrir o arquivo todo.txt para verificação");
        }
        return;
    }
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char current_date[11];
    snprintf(current_date, sizeof(current_date), "%02d/%02d/%04d",
             tm_now->tm_mday, tm_now->tm_mon + 1, tm_now->tm_year + 1900);
    printf("Verificando TODOs vencidos ou a vencer hoje (%s)...\n", current_date);
    int has_overdue = 0;
    char line[1024];
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        char *prazo_start = strstr(line, "Prazo: ");
        if (prazo_start) {
            prazo_start += 7;
            char prazo[32] = {0};
            strncpy(prazo, prazo_start, sizeof(prazo) - 1);
            time_t prazo_time = parse_date(prazo);
            if (prazo_time != -1) {
                if (difftime(now, prazo_time) >= 0) {
                    has_overdue = 1;
                    printf("TODO VENCIDO OU HOJE: %s\n", line);
                }
            }
        }
    }
    fclose(todo_file);
    if (!has_overdue) {
        printf("Nenhum TODO vencido ou a vencer hoje.\n");
    }
}

void list_todo() {
    count = 0;
    printf("Lista de TODOs:\n");
    printf("------------------------------------------------\n");
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        if (access("todo.txt", F_OK) == -1) {
            printf("Nenhum TODO encontrado. Adicione um com 'todo add <tarefa>'.\n");
        } else {
            perror("Erro ao abrir o arquivo todo.txt");
        }
        return;
    }
    char line[1024];
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        printf("%d: %s\n", ++count, line);
    }
    fclose(todo_file);
    if (count == 0) {
        printf("Nenhum TODO encontrado no arquivo.\n");
    }
    printf("------------------------------------------------\n");
}

void remove_todo(const char *args) {
    if (args == NULL || strlen(args) == 0) {
        printf("Uso: todo remove <número do item>\n");
        list_todo();
        return;
    }

    int index = atoi(args);

    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir o arquivo todo.txt para remoção");
        return;
    }

    int current_line = 0;
    while (fgets(lines[current_line], sizeof(lines[current_line]), todo_file) != NULL && current_line < MAX_TODO_LINES) {
        lines[current_line][strcspn(lines[current_line], "\n")] = '\0';
        current_line++;
    }
    fclose(todo_file);

    if (current_line == 0) {
        printf("Nenhum TODO para remover.\n");
        return;
    }

    if (index <= 0 || index > current_line) {
        printf("Número de TODO inválido. Use 'todo list' para ver os números válidos (1-%d).\n", current_line);
        return;
    }

    FILE *temp_file = fopen("todo_temp.txt", "w");
    if (temp_file == NULL) {
        perror("Erro ao criar arquivo temporário");
        return;
    }
    for (int i = 0; i < current_line; i++) {
        if (i != index - 1) {
            fprintf(temp_file, "%s\n", lines[i]);
        }
    }
    fclose(temp_file);

    if (remove("todo.txt") != 0 || rename("todo_temp.txt", "todo.txt") != 0) {
        perror("Erro ao atualizar o arquivo todo.txt");
        return;
    }
    printf("TODO número %d removido com sucesso.\n", index);
}

void handle_add_todo(const char *input) {
    if (input == NULL || strlen(input) == 0) {
        printf("Uso: todo add <descrição da tarefa>\n");
        return;
    }

    char tarefa[512];
    strncpy(tarefa, input, sizeof(tarefa) - 1);
    tarefa[sizeof(tarefa) - 1] = '\0';

    char usuario[128] = {0};
    char prazo[32] = {0};
    char temp_input[256] = {0};

    printf("Digite o nome do responsável (ou deixe vazio para 'Desconhecido'): ");
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler o nome do responsável");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(usuario, temp_input, sizeof(usuario) - 1);
        usuario[sizeof(usuario)-1] = '\0';
    } else {
        strcpy(usuario, "Desconhecido");
    }

    printf("Digite a data de vencimento (dd/mm/aaaa ou deixe vazio para 'Sem prazo'): ");
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler a data de vencimento");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(prazo, temp_input, sizeof(prazo) - 1);
        prazo[sizeof(prazo)-1] = '\0';
    } else {
        strcpy(prazo, "Sem prazo");
    }

    char time_str[26];
    time_t now = time(NULL);
    ctime_r(&now, time_str);
    time_str[24] = '\0';

    FILE *todo_file = fopen("todo.txt", "a");
    if (todo_file == NULL) {
        perror("Erro ao abrir todo.txt");
        return;
    }
    fprintf(todo_file, "[%s] TODO: %s | Usuário: %s | Prazo: %s\n", time_str, tarefa, usuario, prazo);
    fclose(todo_file);
    printf("TODO salvo com sucesso: '%s' por '%s' com prazo '%s'\n", tarefa, usuario, prazo);
}

void edit_todo(const char *args) {
    printf("Funcionalidade 'edit' ainda não implementada.\n");
    printf("Use 'todo vim' para editar o arquivo diretamente.\n");
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
        perror("execvp");
        exit(1);
    }
    else {
        int status;
        waitpid(pid, &status, 0);
    }
}