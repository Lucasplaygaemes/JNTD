#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h> // Para stat (Linux/macOS)

#ifdef _WIN32
#include <direct.h> // Para _getcwd no Windows
#else
#include <unistd.h> // Para getcwd no Linux/macOS
#endif
//Define o tamanho maximo para o buffer de entrada de prompts do usuario
#define MAX_PROMPT_LEN 256
//Define o tamanho maximo do input do ollama + extras
#define MAX_OLLAMA_CMD_LEN 4096 // Aumentado para acomodar o pior caso de prompt longo + prefixos
//Define o tamanho maximo da saida do ollama
#define OLLAMA_BUFFER_SIZE 1024
//define o modelo
#define OLLAMA_MODEL "llama2"
//define o tamanho maximo do historico de comando
#define MAX_HISTORY 50
//define o tamanho maximo do input do usario + extras como calc
#define COMBINED_PROMPT_LEN 2048 // Já estava adequado, mas mantido para clareza
char tarefa[512] = {0};
char usuario[128] = "Desconhecido";
char prazo[32] = "Sem prazo";
char input_copy[1024];
char dir_novo[100];
char *path[100];
char dir_ant[100];
char *command_history[MAX_HISTORY];
int history_count = 0;
static char ult_calc_resu[1024] = "";

// Estrutura de comando
typedef struct {
    const char *key;
    const char *shell_command;
    const char *descri;
} CmdEntry;

// Definição dos comandos (declarado antes das funções que o utilizam)
static const CmdEntry cmds[] = {
    { "ls", "pwd && ls -l", "Diretorio atual e lista arquivos detalhadamente." },
    { ":q", "exit", "Outro forma de sair do JNTD, criado pelo atalho do VIM" },
    { "lsa", "pwd && ls -la", "Igual o listar, mas todos os arquivos (incluindo ocultos)." },
    { "data", "date", "Data e hora atual." },
    { "quem", "whoami", "Nome do usuário atual." },
    { "esp", "df -h .", "Espaço livre do sistema de arquivos atual." },
    { "sysatt?", "pop-upgrade release check", "Verifica se há atualizações do sistema disponíveis (Pop!_OS)." },
    { "sudo", "sudo su", "Entra no modo super usuário (USE COM CUIDADO!)." },
    { "help", NULL, "Lista todos os comandos disponíveis e suas descrições." },
    { "criador", "echo lucasplayagemes é o criador deste codigo.", "Diz o nome do criador do JNTD e 2B." },
    { "2b", NULL, "Inicia uma conversa com a 2B, e processa sua saida." },
    { "log", NULL, "O codigo sempre salva um arquivo log para eventuais casualidades," },
    { "calc", NULL, "Usa a calculadora avançada. Exemplo: calc soma 5 3 ou calc deriv_poly 3 2 2 1 -1 0." },
    { "his", NULL, "Exibe o histórico de comandos digitados." },
    { "cl", "clear", "Limpa o terminal" },
    { "git", NULL, "Mostra o Github do repositorio" },
    { "mkdir", NULL, "Cria um novo diretorio sem nome, nomea-lo será adicionado" },
    { "rscript", NULL, "Roda um script pré definido, coloque cada comando em uma linha" },
    { "sl", "sl", "Easter Egg." },
    { "cd", NULL, "O comando cd, você troca de diretorio, use cd <destino>." },
    { "pwd", "pwd", "Fala o diretorio atual" },
    { "vim", "vim", "O editor no qual fiz todo esse codigo." },
    { "todo", NULL, "Adiciona uma tarefa TODO a ser salva no arquivo todo.txt. Uso: todo <tarefa> | <nome> | <prazo>" },
    { "ct", NULL, "Verifica se há TODOs vencidos ou a vencer hoje." },
    { "tlist", NULL, "List things on TODO.txt." }
};

// Declaração antecipada das funções
void dispatch(const char *user_in);
void handle_ollama_interaction();
void handle_calc_interaction(const char *args);
void display_help();
void log_action(const char *action, const char *details);
int is_safe_command(const char *cmd);
void add_to_history(const char *cmd);
void display_history();
void TODO(const char *input);
void check_todos();
void cd(const char *args);
int jntd_mkdir(const char *args);
void rscript(const char *args);

// Função para verificar segurança de comandos
int is_safe_command(const char *cmd) {
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
        if (strcasecmp(cmd, cmds[i].key) == 0) {
            return 1; // Comando está na lista de permitidos
        }
    }
    return 0; // Comando não reconhecido, não é seguro
}

// Funções para histórico de comandos
void add_to_history(const char *cmd) {
    if (history_count < MAX_HISTORY) {
        command_history[history_count] = strdup(cmd);
        history_count++;
    } else {
        free(command_history[0]);
        for (int i = 1; i < MAX_HISTORY; i++) {
            command_history[i - 1] = command_history[i];
        }
        command_history[MAX_HISTORY - 1] = strdup(cmd);
    }
}

void display_history() {
    printf("Historico de comandos:\n");
    for (int i = 0; i < history_count; i++) {
        printf("  %d: %s\n", i + 1, command_history[i]);
    }
}
void list_todo() {
    printf("Lista de TODOs:\n");
    printf("------------------------------------------------\n");
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir o arquivo todo.txt");
        printf("Não foi possível listar os TODOs. Arquivo não encontrado ou sem permissão.\n");
        return;
    }

    char line[1024];
    int count = 0;
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

void TODO(const char *input) {
    char temp_input[1024] = {0};
    char tarefa[512] = {0};
    char usuario[128] = "Desconhecido";
    char prazo[32] = "Sem prazo";

    do {
	    //limpa o buffer para nova entrada
	    memset(tarefa, 0, sizeof(tarefa));
	    memset(usuario, 0, sizeof(usuario));
	    memset(prazo, 0, sizeof(prazo));
	    //Solicita o assunto da tarefa
	    printf("Digite o assunto da tarefa: ");
	    fflush(stdout);
	    if(fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
		    perror("Erro ao ler o assunto da tarefa");
		    return;
	    }
	    temp_input[strcpy(temp_input, "\n")] = '\0';//Remova a nova linha//
	    strcpy(tarefa, temp_input, sizeof(tarefa) - 1);
	    tarefa[sizeo(tarefa) - 1] = '\0';
	    //Remove espaço em branco no inicio e no fim//
	    char *ptr = tarefa;
	    while (*ptr == ' ') ptr++;
	    memmove(tarefa, ptr, strlen(prt) + 1);
	    char *end = tarefa + strlen(tarefa) - 1;
	    while(end >= tarefa && *end == ' ') *end-- = '\0';

	    if(strlen(tarefa) == 0) {
		    printf("Erro: Nenhuma tarefa fornecida. Operação cancelada.\n");
		    return;
	    }
	    //solicita o nome do responsavel//



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
    char line[1024];
    int has_overdue = 0;
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

// Função para logging de ações
void log_action(const char *action, const char *details) {
    FILE *log_file = fopen("jntd_log.txt", "a");
    if (log_file == NULL) {
        perror("Erro ao abrir arquivo de log");
        return;
    }
    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[24] = '\0'; // Remove newline
    fprintf(log_file, "[%s] %s: %s\n", time_str, action, details);
    fclose(log_file);
}

// Função para interagir com a calculadora
void handle_calc_interaction(const char *args) {
    char calc_command[256];
    char calc_output[1024];
    FILE *calc_pipe;

    ult_calc_resu[0] = '\0'; // Limpa o resultado anterior da calculadora

    // Constrói o comando com argumentos (se houver)
    if (args && strlen(args) > 0) {
        snprintf(calc_command, sizeof(calc_command), "./calc %s", args);
    } else {
        snprintf(calc_command, sizeof(calc_command), "./calc");
    }
    printf("Executando calculadora: %s\n", calc_command);
    calc_pipe = popen(calc_command, "r");
    if (calc_pipe == NULL) {
        perror("Falha ao executar a calculadora com popen");
        fprintf(stderr, "Verifique se ./calc está no diretório atual.\n");
        return;
    }

    printf("-------------- Saída da Calculadora --------------\n");
    while (fgets(calc_output, sizeof(calc_output), calc_pipe) != NULL) {
        printf("%s", calc_output);
        fflush(stdout);
        // Acumula a saída no ult_calc_resu
        strncat(ult_calc_resu, calc_output, sizeof(ult_calc_resu) - strlen(ult_calc_resu) - 1);
        log_action("Calculadora Output", calc_output);
    }
    printf("---------------- Fim da Saída --------------\n");

    int status = pclose(calc_pipe);
    if (status == -1) {
        perror("pclose falhou para o pipe da calculadora");
    } else {
        if (WIFEXITED(status)) {
            printf("Calculadora terminou com status: %d\n", WEXITSTATUS(status));
        }
    }
}

// Função para interação com Ollama/2B
void handle_ollama_interaction() {
    char user_prompt[MAX_PROMPT_LEN];
    char combined_prompt[COMBINED_PROMPT_LEN];
    char ollama_full_command[MAX_OLLAMA_CMD_LEN];
    char ollama_output_line[OLLAMA_BUFFER_SIZE];
    FILE *ollama_pipe;

    printf("Digite o seu prompt para a 2B, a saida sera processada como comandos\n");
    printf("IA prompt> ");
    fflush(stdout);

    if (fgets(user_prompt, sizeof(user_prompt), stdin) == NULL) {
        perror("falha ao ler o prompt do usuario para a 2B");
        return;
    }

    user_prompt[strcspn(user_prompt, "\n")] = '\0';
    log_action("User Prompt to 2B", user_prompt);

    if (user_prompt[0] == '\0') {
        printf("prompt vazio, nenhuma interação com o ollama\n");
        return;
    }
    
    // Adiciona o ultimo resultado da calculadora ao prompt, se houver
    if (strlen(ult_calc_resu) > 0) {
        snprintf(combined_prompt, sizeof(combined_prompt),
                 "%s\n[Ultimo resultado da calculadora: %s]", user_prompt, ult_calc_resu);
        snprintf(ollama_full_command, sizeof(ollama_full_command),
                 "ollama run %s \"%s\"", OLLAMA_MODEL, combined_prompt);
    } else {
        snprintf(ollama_full_command, sizeof(ollama_full_command),
                 "ollama run %s \"%s\"", OLLAMA_MODEL, user_prompt);
    }

    printf("executando ollama com: %s\n", ollama_full_command);
    printf("aguardando resposta da 2B...\n");
    printf("-------------- 2B output --------------\n");

    ollama_pipe = popen(ollama_full_command, "r");
    if (ollama_pipe == NULL) {
        perror("falha ao executar o comando ollama com popen");
        fprintf(stderr, "verifique se ollama está no path e se a 2B está disponivel\n");
        return;
    }

    while (fgets(ollama_output_line, sizeof(ollama_output_line), ollama_pipe) != NULL) {
        printf("%s", ollama_output_line);
        fflush(stdout);
        log_action("2B Output", ollama_output_line);

        ollama_output_line[strcspn(ollama_output_line, "\n")] = '\0';
        if (ollama_output_line[0] != '\0') {
            if (strncmp(ollama_output_line, "CMD:", 4) == 0) {
                const char *cmd = ollama_output_line + 4;
                if (is_safe_command(cmd)) {
                    printf(">>> Executando comando seguro da 2B: '%s'\n", cmd);
                    dispatch(cmd);
                } else {
                    printf(">>> AVISO: Comando '%s' da 2B não é seguro. Ignorado.\n", cmd);
                    printf("    Digite 'help' para ver comandos permitidos.\n");
                }
            } else {
                printf(">>> Texto da 2B (não comando): '%s'\n", ollama_output_line);
            }
        }
    }
    printf("---------------- Fim da fala da 2B --------------\n");

    int status = pclose(ollama_pipe);
    if (status == -1) {
        perror("pclose falhou para o pipe do ollama");
    } else {
        if (WIFEXITED(status)) {
            printf("processo do ollama terminou com o status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Processo ollama terminou com %d\n", WTERMSIG(status));
        }
    }
}

void display_help() {
    printf("Comandos disponíveis:\n");
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
        printf("  %-15s: %s\n", cmds[i].key, cmds[i].descri);
    }
}

void cd(const char *args) {
    // Verifica se o argumento (caminho do diretório) foi fornecido
    if (args && strlen(args) > 0) {
        char old_dir[256];
        char new_dir[256];
        char args_copy[256];
    // Obtém o diretório atual antes da mudança
        if (getcwd(old_dir, sizeof(old_dir)) == NULL) {
            perror("Erro ao obter o diretorio atual antes da mudança");
            old_dir[0] = '\0';
        }
        // Faz uma cópia do argumento para evitar modificação direta
        strncpy(args_copy, args, sizeof(args_copy) - 1);
        args_copy[sizeof(args_copy) - 1] = '\0';
        // Remove newline ou espaços extras, se houver
        args_copy[strcspn(args_copy, "\n")] = '\0';
        // Tenta mudar de diretório
        int result = chdir(args_copy);
        if (result == 0) {
            // Obtém o novo diretório atual após a mudança
            if (getcwd(new_dir, sizeof(new_dir)) == NULL) {
                perror("Erro ao obter o novo diretorio atual");
                // Continua sem mostrar o novo caminho
                new_dir[0] = '\0';
            }
            printf("Diretorio alterado com sucesso. Antes '%s', Agora '%s'\n",
                   old_dir[0] ? old_dir : "desconhecido",
                   new_dir[0] ? new_dir : "desconhecido");
        } else {
            perror("Erro ao mudar de diretorio");
            printf("Permanece no diretorio atual: '%s'\n", 
                   old_dir[0] ? old_dir : "desconhecido");
        }
    } else {
        printf("Erro: Caminho do diretorio não fornecido. Uso: cd <caminho>\n");
    }
}

int jntd_mkdir(const char *args) {
    // Verifica se o argumento (nome do diretório) foi fornecido
    if (args && strlen(args) > 0) {
        char current_dir[256];
        char mkdir_command[256];
        char full_path[512];
        // Obtém o diretório de trabalho atual
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            perror("Erro obter o diretorio atual");
            current_dir[0] = '\0';
            snprintf(full_path, sizeof(full_path), "%s", args);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", current_dir, args);
        }
        // Verifica se o diretório já existe
        struct stat st;
        if (stat(args, &st) == 0) {
            printf("Erro: O diretorio '%s' já existe em '%s'.\n", 
                   args, current_dir[0] ? current_dir : "diretorio atual desconhecido");
        } else {
            // Constrói o comando para criar o diretório
            snprintf(mkdir_command, sizeof(mkdir_command), "mkdir \"%s\"", args);
            printf("Criando diretorio '%s' em '%s'...\n", 
                   args, current_dir[0] ? current_dir : "Diretorio atual desconhecido");
            // Executa o comando mkdir via system
            int status = system(mkdir_command);
            if (status == -1) {
                perror("Erro ao executar o comando mkdir");
            } else {
                if (WIFEXITED(status)) {
                    int exit_code = WEXITSTATUS(status);
                    printf("Comando mkdir terminou com o status '%d'.\n", exit_code);
                    if (exit_code == 0) {
                        // Verifica novamente se o diretório foi criado
                        if (stat(args, &st) == 0) {
                            printf("Diretorio criado com sucesso em '%s'.\n", full_path);
                        } else {
                            printf("Erro: Diretorio não foi criado, mesmo com codigo de saida 0\n");
                        }
                    }
                }
            }
        }
    } else {
        printf("Erro: Nome do diretório não fornecido. Uso: mkdir <nome>\n");
    }
    return 0; // Adicionado retorno para evitar warnings
}

void rscript(const char *args) {
    // Verifica se o argumento (nome do arquivo de script) foi fornecido
    if (args && strlen(args) > 0) {
        FILE *script_file = fopen(args, "r");
        if (script_file == NULL) {
            perror("Erro ao abrir o arquivo de script");
            printf("Verifique se o arquivo: '%s' se encontra no diretorio, e se você tem permissão para lê-lo\n", args);
        } else {
            char script_line[128];
            printf("Executando comandos do script '%s':\n", args);
            printf("-----------------------------------------------\n");
            while (fgets(script_line, sizeof(script_line), script_file) != NULL) {
                script_line[strcspn(script_line, "\n")] = '\0';
                if (script_line[0] != '\0' && script_line[0] != '#') {
                    printf("Executando %s\n", script_line);
                    dispatch(script_line);
                }
            }
            printf("------------------------------------------------\n");
            printf("Fim da execução do script '%s'.\n", args);
            fclose(script_file);
        }
    } else {
        printf("Erro: Nome do arquivo de script não fornecido. Uso: rscript <nome_arquivo>\n");
    }
}

void dispatch(const char *user_in) {
    char input_copy[128];
    strncpy(input_copy, user_in, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';

    char *token = strtok(input_copy, " ");
    if (token == NULL) return;

    char *args = strtok(NULL, ""); // Pega o resto da string como argumentos

    // Adiciona ao histórico e log
    add_to_history(user_in);
    log_action("User Input", user_in);

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
        if (strcasecmp(token, cmds[i].key) == 0) {
            if (strcmp(cmds[i].key, "help") == 0) {
                display_help();
            } else if (strcasecmp(cmds[i].key, "mkdir") == 0) {
                jntd_mkdir(args); // Passa os argumentos para mkdir
            } else if (strcasecmp(cmds[i].key, "2b") == 0) {
                handle_ollama_interaction();
            } else if (strcasecmp(cmds[i].key, "calc") == 0) {
                handle_calc_interaction(args);
            } else if (strcasecmp(cmds[i].key, "cd") == 0) {
                cd(args); // Passa os argumentos para cd
            } else if (strcasecmp(cmds[i].key, "git") == 0) {
                printf("O Github do criador e do projeto é 'https://github.com/Lucasplaygaemes/JNTD'\n");
            } else if (strcasecmp(cmds[i].key, "his") == 0) { // Corrige de "historico" para "his" conforme cmds
                display_history();
            } else if (strcasecmp(cmds[i].key, "todo") == 0) {
                TODO(args);
            } else if (strcasecmp(cmds[i].key, "ct") == 0) {
                check_todos();
	    } else if (strcasecmp(cmds[i].key, "tlist") == 0) {
		list_todo();
	    } else if (strcasecmp(cmds[i].key, "rscript") == 0) {
                rscript(args); // Passa os argumentos para rscript
            } else if (cmds[i].shell_command != NULL) {
                // Executa comandos shell definidos na tabela
                system(cmds[i].shell_command);
            }
            return;
        }
    }
    printf("Comando '%s' não reconhecido. Use 'help' para ver os comandos disponíveis.\n", token);
}

int main(void) {
    printf("Iniciando o JNTD...\n");
    check_todos();
    char buf[512];
    printf("Digite um comando. Use 'help' para ver as opções ou 'sair' para terminar.\n");
    while (printf("> "), fgets(buf, sizeof(buf), stdin) != NULL) {
        buf[strcspn(buf, "\n")] = '\0'; // Remove newline
        if (strcmp(buf, "sair") == 0) {
            break;
        } else if(strcmp(buf, ":q") == 0) {
            break;
        }
        if (buf[0] == '\0') { // Usuário apenas apertou Enter
            continue;
        }
        dispatch(buf);
    }
    printf("Saindo....\n");
    return 0;
}
