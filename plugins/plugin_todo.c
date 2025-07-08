#include <stdio.h>
#include <stdlib.h>
#include <string.h> 
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include "plugin.h"

#define MAX_TODO_LINES 100
#define max_todo 100
static char lines[MAX_TODO_LINES][1024];
int count;

void check_todos();
void handle_add_todo();
void list_todo();
void remove_todo();
void edit_todo();
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


void check_todos() {
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir o arquivo todo.txt para verificação");
        printf("Não foi possível verificar os TODOs. Arquivo não encontrado ou sem permissão.\n");
        return;
    }
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);
    char current_date[11];
    snprintf(current_date, sizeof(current_date), "%02d/%02d/%04d",
             tm_now->tm_mday, tm_now->tm_mon + 1, tm_now->tm_year + 1900);
    printf("Verificando TODOs vencidos ou a vencer hoje (%s)...\n", current_date);
    int has_overdue = 0;
    char line[1024]; // Buffer temporário para cada linha
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        line[strcspn(line, "\n")] = '\0'; // Remove nova linha
        // Extrai os campos da linha
        char *prazo_start = strstr(line, "Prazo: ");
        if (prazo_start) {
            prazo_start += 7; // Pula "Prazo: "
            char prazo[32] = {0};
            char *end = strchr(prazo_start, '\n');
            if (end) *end = '\0'; // Termina a string no final da linha
            strncpy(prazo, prazo_start, sizeof(prazo) - 1);
            time_t prazo_time = parse_date(prazo);
            if (prazo_time != -1) { // Data válida
                if (difftime(now, prazo_time) >= 0) { // Data atual >= prazo
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
    count = 0; // Reinicia o contador global
    printf("Lista de TODOs:\n");
    printf("------------------------------------------------\n");
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir o arquivo todo.txt");
        printf("Não foi possível listar os TODOs. Arquivo não encontrado ou sem permissão.\n");
        return;
    }
    char line[1024]; // Buffer temporário para cada linha
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        line[strcspn(line, "\n")] = '\0'; // Remove nova linha do final
        printf("%d: %s\n", ++count, line);
    }
    fclose(todo_file);
    if (count == 0) {
        printf("Nenhum TODO encontrado no arquivo.\n");
    }
    printf("------------------------------------------------\n");
}

void remove_todo() {
    list_todo(); // Mostra a lista de TODOs para o usuário escolher
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir o arquivo todo.txt");
        printf("Não foi possível remover TODOs. Arquivo não encontrado ou sem permissão.\n");
        return;
    }
    if (count >= max_todo) {
	    printf("Limite de TODOs atingido!\n");
            return;
    }
    // Lê todas as linhas para um buffer temporário
    // Limite de 100 TODOs para simplificar
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), todo_file) != NULL && count < 100) {
        lines[count][strcspn(lines[count], "\n")] = '\0';
        count++;
    }
    fclose(todo_file);
    if (count == 0) {
        printf("Nenhum TODO para remover.\n");
        return;
    }

    // Solicita o número do TODO a ser removido
    char temp_input[256];
    printf("Digite o número do TODO a ser removido (1-%d, ou 0 para cancelar): ", count);
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler entrada");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    int index = atoi(temp_input);
    if (index <= 0 || index > count) {
        printf("Operação cancelada ou número inválido.\n");
        return;
    }

    // Escreve todas as linhas, exceto a escolhida, de volta ao arquivo
    FILE *temp_file = fopen("todo_temp.txt", "w");
    if (temp_file == NULL) {
        perror("Erro ao criar arquivo temporário");
        printf("Não foi possível remover o TODO.\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (i != index - 1) { // Ignora a linha a ser removida
            fprintf(temp_file, "%s\n", lines[i]);
        }
    }
    fclose(temp_file);

    // Substitui o arquivo original pelo temporário
    if (remove("todo.txt") != 0 || rename("todo_temp.txt", "todo.txt") != 0) {
        perror("Erro ao atualizar o arquivo todo.txt");
        printf("Não foi possível completar a remoção.\n");
        return;
    }
    printf("TODO número %d removido com sucesso.\n", index);
}



void handle_add_todo(const char *input) {

    char tarefa[512] = {0};

    char usuario[128] = {0};

    char prazo[32] = {0};

    char temp_input[256] = {0};



    do {
        // Limpa os buffers para nova entrada
        memset(tarefa, 0, sizeof(tarefa));
        memset(usuario, 0, sizeof(usuario));

        memset(prazo, 0, sizeof(prazo));



        // Solicita o assunto da tarefa

        printf("Digite o assunto da tarefa: ");

        fflush(stdout);

        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {

            perror("Erro ao ler o assunto da tarefa");

            return;

        }

        temp_input[strcspn(temp_input, "\n")] = '\0'; // Remove nova linha

        strncpy(tarefa, temp_input, sizeof(tarefa) - 1);

        tarefa[sizeof(tarefa) - 1] = '\0';

        // Remove espaços em branco do início e fim

        char *ptr = tarefa;

        while (*ptr == ' ') ptr++;

        memmove(tarefa, ptr, strlen(ptr) + 1);

        char *end = tarefa + strlen(tarefa) - 1;

        while (end >= tarefa && *end == ' ') *end-- = '\0';



        if (strlen(tarefa) == 0) {

            printf("Erro: Nenhuma tarefa fornecida. Operação cancelada.\n");

            return;

        }



        // Solicita o nome do responsável

        printf("Digite o nome do responsável (ou deixe vazio para 'Desconhecido'): ");

        fflush(stdout);

        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {

            perror("Erro ao ler o nome do responsável");

            return;

        }

        temp_input[strcspn(temp_input, "\n")] = '\0'; // Remove nova linha

        if (strlen(temp_input) > 0) {

            strncpy(usuario, temp_input, sizeof(usuario) - 1);

            usuario[sizeof(usuario) - 1] = '\0';

            ptr = usuario;

            while (*ptr == ' ') ptr++;

            memmove(usuario, ptr, strlen(ptr) + 1);

            end = usuario + strlen(usuario) - 1;

            while (end >= usuario && *end == ' ') *end-- = '\0';

        } else {

            strcpy(usuario, "Desconhecido");

        }



        // Solicita a data de vencimento

        printf("Digite a data de vencimento (dd/mm/aaaa ou deixe vazio para 'Sem prazo'): ");

        fflush(stdout);

        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {

            perror("Erro ao ler a data de vencimento");

            return;

        }

        temp_input[strcspn(temp_input, "\n")] = '\0'; // Remove nova linha

        if (strlen(temp_input) > 0) {

            strncpy(prazo, temp_input, sizeof(prazo) - 1);

            prazo[sizeof(prazo) - 1] = '\0';

            ptr = prazo;

            while (*ptr == ' ') ptr++;

            memmove(prazo, ptr, strlen(ptr) + 1);

            end = prazo + strlen(prazo) - 1;

            while (end >= prazo && *end == ' ') *end-- = '\0';

        } else {

            strcpy(prazo, "Sem prazo");

        }



        // Obtém o tempo atual para o carimbo de criação

        char time_str[26];

        time_t now = time(NULL);

        ctime_r(&now, time_str);

        time_str[24] = '\0'; // Remove newline do final da string de tempo



        // Salva no arquivo

        FILE *todo_file = fopen("todo.txt", "a");

        if (todo_file == NULL) {

            perror("Erro ao abrir todo.txt");

            printf("Não foi possível salvar o TODO. Verifique as permissões ou o diretório.\n");

            return;

        }

        fprintf(todo_file, "[%s] TODO: %s | Usuário: %s | Prazo: %s\n", time_str, tarefa, usuario, prazo);

        fclose(todo_file);

        printf("TODO salvo com sucesso: '%s' por '%s' com prazo '%s' em [%s]\n", tarefa, usuario, prazo, time_str);



        // Pergunta se o usuário deseja adicionar outro TODO

        printf("Deseja adicionar outro TODO? (s/n): ");

        fflush(stdout);

        if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {

            perror("Erro ao ler resposta");

            return;

        }

        temp_input[strcspn(temp_input, "\n")] = '\0';

    } while (temp_input[0] == 's' || temp_input[0] == 'S');

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
