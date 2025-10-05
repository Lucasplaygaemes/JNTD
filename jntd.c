#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/stat.h>
#include <pthread.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <curl/curl.h>
#include <stdint.h>
#include <stdbool.h>
#include <termios.h> 
#include <ctype.h>
#include <openssl/sha.h>
#include "plugins/plugin_todo.c"
#include "plugin.h"

#ifdef _WIN32
#include <windows.h> // Para Sleep()
#endif
#ifdef _WIN32
#include <direct.h> // Para _getcwd no Windows
#else
#include <unistd.h> // Para getcwd no Linux/macOS
#endif

#define MAX_ALIASES 128
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

char input_copy[1024];
char dir_novo[100];
char *path[100];
char dir_ant[100];
char *command_history[MAX_HISTORY];
int history_count = 0;
char buf[512];
int linhazinhas[100];
char quiz[1024];
int countar = 0;
int line_number;
const char* quiz_file = "quiz.txt";
char time_str[26] = {0};
int seconds;
volatile int timer_running = 0;
volatile int quiz_timer_running = 0;
volatile int current_timer_seconds = 0;
int plugin_count = 0;

pthread_t timer_thread;
pthread_t quiz_thread;

struct termios orig_termios;

//Estrutura dos comandos//
typedef struct {
    const char *key;
    const char *shell_command;
    const char *descri;
} CmdEntry;

//Estrutura dos Alias
typedef struct {
	char *name;
	char *command;
} Alias;

Alias alias_list[MAX_ALIASES];
int alias_count = 0;

//Estrutura dos plugins//
typedef struct {
        void *handle;
        Plugin *plugin;
} LoadedPlugin;

LoadedPlugin loaded_plugins[20];

//Estrutura de memoria
typedef struct {
	char *memory;
	size_t size;
} MemoryStruct;

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
    { "his", NULL, "Exibe o histórico de comandos digitados." },
    { "cl", "clear", "Limpa o terminal" },
    { "git", NULL, "Mostra o link para Github do repositorio, além da ultima commit, mas a commit não funciona sempre. Pois depende do git do sistema, que pode estar ligado a outro repo." },
    { "mkdir", NULL, "Cria um novo diretorio sem nome, nomea-lo será adicionado" },
    { "cp", NULL, "Copia arquivos e diretorios, use: cp <origem> <destino>." },
    { "rm", NULL, "Remove arquivos ou diretorio, use rm <alvo>. ATENÇÃO, se o arquivo ou a pasta não foram removidos com o comando padrão, use rm -rf <nome>, assim ele apagará também todas as subpastas." },
    { "mv", NULL, "Move o arquivos ou renomeia arquivos e diretorios, use: <origem> <destino>." },
    { "rscript", NULL, "Roda um script pré definido, coloque cada comando em uma linha" },
    { "sl", "sl", "Easter Egg." },
    { "cd", NULL, "O comando cd, você troca de diretorio, use cd <destino>." },
    { "pwd", "pwd", "Fala o diretorio atual" },
    { "vim", "vim", "Abre o editor, aceita nome para editar um arquivo." },
    { "todo", NULL, "Gerencia tarefas (add, list, remove, edit, check, vim)." },
    { "quiz", NULL, "Mostra todas as perguntas do quiz do integrado." },
    { "quizt", NULL, "Define o intervalo de tempo entre os QUIZ'es." },
    { "quizale", NULL, "Uma pergunta aleatoria do QUIZ é feita." },
    { "timer", NULL, "Um simples timer." },
    { "cp_di", NULL, "Um copy, use com <de qual arquivo para qual>" },
    { "alias", NULL, "Adiciona alias." },
    { "a2", NULL, "Inicia a A2, um editor de texto simples do JNTD." },
    { "download", NULL, "Uma função de download, <use com download, depois irá pedir o nome do arquivo.>" },
    { "buscar", NULL, "Uma função para buscar coisas pelo JNTD." },
    { "elinks", "elinks", "Elinks é um código que te permite fazer pesquisas na internet sem sair do terminal. Todos os direitos vão para o criador." },
    { "awrit", "awrit", "Awrit é um codigo que te permite usar o Chorimium pelo TERMINAL! Isso, sem que você saia dele, Todos os direitos vão para o craidor," },
    { "hash", NULL, "Verifica ou gera hashes SHA-256 para arquivos." }
};

// Declaração antecipada das funções
void dispatch(const char *user_in);
void handle_ollama_interaction();
void enable_raw_mode();
void disable_raw_mode();
void display_help();
void log_action(const char *action, const char *details);
int is_safe_command(const char *cmd);
void add_to_history(const char *cmd);
void display_history();
void cd(const char *args);
int jntd_mkdir(const char *args);
void rscript(const char *args);
void func_quiz();
char *read_random_line();
void *timer_background(void *arg);
void *quiz_timer_background(void *arg);
void load_plugins();
void execute_plugin(const char* name, const char* args);
void save_aliases_to_file();
void handle_hash_command();

//implementação dos plugins, sempre antes do dispatch//
void load_plugins() {
    DIR *dir = opendir("plugins");
    if (!dir) {
        printf("Pasta de plugins não encontrada. Criando...\n");
        mkdir("plugins", 0700);  // Cria a pasta se não existir
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && plugin_count < 20) {
        if (strstr(ent->d_name, ".so")) {
		char plugin_path[512];
		snprintf(plugin_path, sizeof(plugin_path), "plugins/%s", ent->d_name);


		//carrega a biblioteca compartilhada//
		void *handle = dlopen(plugin_path, RTLD_LAZY);
		if (!handle) {
			fprintf(stderr, "Erro ao carregar o plugin %s: %s\n", plugin_path, dlerror());
			continue;
		}
		// Procura a função registrar plugin dentro da biblioteca//
		Plugin *(*register_func)();//declara uma função de pointer//
		register_func = dlsym(handle, "register_plugin");
		if (!register_func) {
			fprintf(stderr, "Erro: Plugin %s não tem função register_plugin.\n", plugin_path);
			dlclose(handle);
			continue;
		}
		//chama a função para conseguir a data do plugin//
		Plugin *p = register_func();
		if (!p) {
			fprintf(stderr, "Erro: register_plugin em %s retornou NULL.\n", plugin_path);
			dlclose(handle);
			continue;
		}
		//armazena o handle e a data do plugin//
		loaded_plugins[plugin_count].handle = handle;
		loaded_plugins[plugin_count].plugin = p;
		plugin_count++;
		printf("Plugin '%s' carregado com sucesso.\n", p->name);
	}
    }
    closedir(dir);
}

void handle_alias_command(const char *args) {
	if (!args || strlen(args) == 0) {
		printf("Uso: alias <nome> \"<comando>\"\n");
		printf("Exemplo: alias listar \"ls -la\"\n");
		printf("\nAlaises atuais:\n");
		for (int i = 0; i < alias_count; i++) {
			printf("  %s = \"%s\"\n", alias_list[i].name, alias_list[i].command);
		}
		return;
	}
	if (alias_count >= MAX_ALIASES) {
		printf("Erro: Limite de aliases atingido.\n");
		return;
	}
	char args_copy[256];
	strncpy(args_copy, args, sizeof(args_copy) - 1);

	char *name = strtok(args_copy, " ");
	char *command = strtok(NULL, "");
	if (!name || !command) {
		printf("Erro: Formato Invalido. Use: alias <nome>\"<comando>\"\n");
		return;
	}
	
	if (command[0] == '"' || command[0] == '\'') {
		command++;
		char *end = strrchr(command, command[-1]);
		if (end) *end = '\0';
	}
	alias_list[alias_count].name = strdup(name);
	alias_list[alias_count].command = strdup(command);
	
	if (!alias_list[alias_count].name || !alias_list[alias_count].command) {
		printf("Erro de alocação de memoria!\n");
		free(alias_list[alias_count].name);
		free(alias_list[alias_count].command);
		return;
	}
	printf("Alias criado: %s -> %s\n", name, command);
	alias_count++;
	save_aliases_to_file();
}

void load_aliases_from_file() {
	FILE *file = fopen("aliases.txt", "r");
	if (!file) {
		return;
	}
	char line[512];
	while (fgets(line, sizeof(line), file) && alias_count < MAX_ALIASES) {
		line[strcspn(line, "\n")] = 0;
		char *name = strtok(line, "=");
		char *command = strtok(NULL, "");
		if (name && command) {
			alias_list[alias_count].name = strdup(name);
			alias_list[alias_count].command = strdup(command);
			alias_count++;
		}
	}
	fclose(file);
}

void save_aliases_to_file() {
	FILE *file = fopen("aliases.txt", "w");
	if (!file) {
		perror("Erro ao salvar aliases");
		return;
	}
	for (int i = 0; i < alias_count; i++) {
		fprintf(file, "%s=%s\n", alias_list[i].name, alias_list[i].command);
	}
	fclose(file);
}

//Função para executar os plugins
void execute_plugin(const char* name, const char* args) {
    for (int i = 0; i < plugin_count; i++) {
        if (strcmp(name, loaded_plugins[i].plugin->name) == 0) {
            // Desativa o modo raw para que o plugin possa usar I/O padrão
            disable_raw_mode(); 
            
            printf("Executando plugin: %s com argumentos: %s\n", name, args ? args : "");
            loaded_plugins[i].plugin->execute(args);
            
            // Reativa o modo raw para o shell principal
            enable_raw_mode(); 
            return;
        }
    }
    printf("Plugin '%s' não encontrado.\n", name);
}

typedef struct {
	FILE *fp;
	size_t dl_total;
} dl_status;


//função para escrever no arquivo//
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
	dl_status *status = (dl_status *)stream;
	size_t written = fwrite(ptr, size, nmemb, status->fp);

	status->dl_total += written;

    // Converte o total de bytes para megabytes
    double mb_total = (double)status->dl_total / (1024.0 * 1024.0);

    // Imprime o progresso formatado em MB, usando  para atualizar a linha
	printf("\rDownload: %.2f MB        ", mb_total);
	fflush(stdout);
	return written;

}

//Função para fazer o download da função//
bool download_file(char *url, char *filename) {
	bool sucess = true;

	CURL *curl_handle = curl_easy_init();

	curl_easy_setopt(curl_handle, CURLOPT_URL, url);
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);

	FILE *pagefile = fopen(filename, "wb");
	
	if (!pagefile) {
		perror("Erro ao baixar o arquivo");
		curl_easy_cleanup(curl_handle);
		return false;
	}

	dl_status status;
	status.dl_total = 0;
	status.fp = pagefile;
	
curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &status);
	
	for (int i = 0; i < 5; i++) {
		CURLcode res = curl_easy_perform(curl_handle);
		if (res != CURLE_OK) {
			sucess = false;
			fprintf(stderr, "Falha na tentativa %d: %s\n", i + 1, curl_easy_strerror(res));
		} else {
			sucess = true;
		}
		if (sucess) {
			break;
		}
		sleep(1);
	}
	fclose(pagefile);
	
	if (!sucess) {
		remove(filename);
	}
	printf("\n");
    return sucess;
}

//Função que faz copia entre arquvios de texto.
int copy_f_t() {
	FILE *fptr1, *fptr2;
	char filename[100];
	int c;
	printf("Coloque o nome do arquivo para copiar\n");
	scanf("%s", filename);
	fptr1 = fopen(filename, "r");
	if (fptr1 == NULL) {
		printf("Não foi possivel abrir o arquivo %s\n", filename);
		exit(1);
	}
	printf("Coloque o nome do arquivo para ser escrito\n");
	scanf("%s", filename);

	fptr2 = fopen(filename, "w");
	if (fptr2 == NULL) {
		printf("Não foi possivel o arquivo %s\n", filename);
		exit(1);
	}
	while ((c = fgetc(fptr1)) != EOF) {
		fputc(c, fptr2);
	}
	printf("Conteudo copiado para %s\n", filename);
	fclose(fptr1);
	fclose(fptr2);
	return 0;
}

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
        command_history[history_count] = strdup(cmd); // Aloca memoria
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

void quiz_aleatorio() {
    printf("Você deseja jogar o QUIZ? (s/n)\n");
    char respostas[256];
    if (fgets(respostas, sizeof(respostas), stdin) != NULL) {
        respostas[strcspn(respostas, "\n")] = '\0'; // Remove newline character
        if (strcasecmp(respostas, "s") == 0 || strcasecmp(respostas, "sim") == 0) {
            char* linha_aleatoria = read_random_line();
            if (linha_aleatoria != NULL) {
                printf("Pergunta aleatória: %s\n", linha_aleatoria);
                free(linha_aleatoria); // Libera a memória alocada por read_random_line
            } else {
                printf("Erro ao obter uma pergunta aleatória.\n");
            }
        } else {
            printf("Não foi possível identificar a resposta\n");
        }
    } else {
        printf("Erro ao ler a resposta\n");
    }
}

void timer() {
	if (timer_running) {
		printf("Um timer já está em execução! Cancele-o primeiro com 'timer cancel'.\n");
		return;
	}

	printf("Um simples timer, que agora roda em segundo plano, qual a duração dele? (0 para cancelar)\n");
	scanf("%d", &seconds);
	if (seconds <= 0) {
		printf("O Timer foi cancelado.\n");
		return;
	}
	//Cria a thread para o timer//
	#ifdef _WIN32
	//No windows, criar thread com o CreateThread (precisa ser implementado se necessario)
	printf("Thread no windows não implementado ainda, usando modo bloquante temporariamente.\n");
	timer_background(&seconds);
	#else
	if (pthread_create(&timer_thread, NULL, timer_background, &seconds) != 0) {
		perror("Erro ao criar thread do timer\n");
		return;
	}
	//Opcional: desanexar a thread para não precisa de join//
	pthread_detach(timer_thread);
	#endif
}

void quiz_timer() {
	if (quiz_timer_running) {
		printf("Um quiz timer já está em execução! Cancele-o primeiro com 'quizt cancel'.\n");
		return;
	}
	printf("Qual o intervalo de tempo QUIZES em segundos? (Aperte enter para o padrão, 10[600s] min)\n");
	char input[256];
	if (fgets(input, sizeof(input), stdin) == NULL || input[0] == '\n') {
		seconds = 600;
	} else {
		seconds = atoi(input);
		if (seconds <= 0) seconds = 600;
	}

	#ifdef _WIN32
	printf("Thread no windows não implementado ainda, usando o modo bloqueado temporariamente.\n");
	quiz_timer_background(&seconds);//simulação roda na thread principal por agr
	#else
	if (pthread_create(&quiz_thread, NULL, quiz_timer_background, &seconds) != 0) {
		perror("Erro ao criar thread do quiz_timer");
		return;
	}
	pthread_detach(quiz_thread);
	#endif
}

//função para cancelar timers
void cancel_timer(const char *type) {
	if (strcmp(type, "timer") == 0) {
		if (timer_running) {
			timer_running = 0;
			printf("Timer cancelado.\n");
		} else {
			printf("Nenhum timer em execução.\n");
		}
	} else if (strcmp(type, "quizt") == 0) {
		if (quiz_timer_running) {
			quiz_timer_running = 0;
			printf("Quiz timer cancelado.\n");
			} else {
				printf("Nenhum quiz timer em execução.\n");
		}
	}
}

void *timer_background(void *arg) {
	int seconds = *(int*)arg;//Recebe o tempo em segundos
	current_timer_seconds = seconds;
	timer_running = 1;
	printf("\rTimer iniciado em segundo plano: %d segundos\n", seconds);
	while (current_timer_seconds > 0 && timer_running) {
		// int horas = current_timer_seconds / 3600;
		// int minutos = (current_timer_seconds % 3600) / 60;
		// int segundos = current_timer_seconds % 60;
		//printf("\rTimer: %02d:%02d:%02d\n", horas, minutos, segundos);
		//fflush(stdout);
		#ifdef _WIN32
		sleep(1000);// 1 segundo no windows//
		#else
		sleep(1);
		#endif
		current_timer_seconds--;
	}
	if (timer_running) {
		printf("\rO Tempo acabou!\n");
		printf("Aperte enter para continuar\n");
		return NULL;
	} else {
		printf("\rTimer cancelado!\n");
	}
	timer_running = 0;
	return NULL;
}

void *quiz_timer_background(void *arg) {
	int seconds = *(int*)arg; //Recebe o tempo em segudos//
	quiz_timer_running = 1;
	printf("Quiz Timer iniciado em segundo plano %d segundo\n", seconds);
	while (quiz_timer_running) {
		#ifdef _WIN32
		Sleep(seconds * 1000);
		#else
		sleep(seconds);
		#endif

		if (quiz_timer_running) {
			printf("\n[Quiz Timer] Tempo para uma pergunta!\n");
			quiz_aleatorio();
			printf("> "); //Reexibe o prompt para o usuario//
			fflush(stdout);
		}
	}
	printf("Quiz Timer encerrado.\n");
	return NULL;
}

void func_quiz() {
    printf("Lista de QUIZ's:\n");
    printf("------------------------------------------------\n");
    FILE *quiz_f = fopen("quiz.txt", "r");
    if (quiz_f == NULL) {
        perror("Erro ao abrir o arquivo quiz.txt");
        printf("Não foi possível listar os QUIZ's. Arquivo não encontrado ou sem permissão.\n");
        return;
    }
    while (fgets(quiz, sizeof(quiz), quiz_f) != NULL) {
        quiz[strcspn(quiz, "\n")] = '\0'; // Remove nova linha do final
        printf("%d: %s\n", ++countar, quiz);
    }
    fclose(quiz_f);
    if (countar == 0) {
	   printf("Nenhum QUIZ encontrado no arquivo.\n");
    }
    printf("------------------------------------------------\n");
}

int contar_linhas() {
	FILE *quizlin = fopen("quiz.txt", "r");
	if (quizlin == NULL) {
		perror("Erro ao abrir o arquivo quiz.txt");
		printf("Não foi possível listar os QUIZ's. Arquivo não encontrado ou sem permissão.\n");
		return 0;
	}
	int linhaslin = 0;
	char buffin1[1024];
	while (fgets(buffin1, sizeof(buffin1), quizlin) != NULL) {
		linhaslin++;
	}
	fclose(quizlin);
	return linhaslin;
}

void a2(const char *cmd_args) {
	pid_t pid = fork();
	int status;

	if (pid == -1) {
		perror("Erro com o FORK\n");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		char *argv[MAX_PROMPT_LEN / 2 + 2]; // Max tokens + program name + NULL
		int i = 0;
		argv[i++] = "./a2";

		if (cmd_args && strlen(cmd_args) > 0) {
			char args_copy[MAX_PROMPT_LEN];
			strncpy(args_copy, cmd_args, sizeof(args_copy) - 1);
			args_copy[sizeof(args_copy) - 1] = '\0';

			char *arg_token = strtok(args_copy, " ");
			while (arg_token != NULL && i < (MAX_PROMPT_LEN / 2 + 1)) {
				argv[i++] = arg_token;
				arg_token = strtok(NULL, " ");
			}
		}
		argv[i] = NULL;

		execvp("./a2", argv);
		perror("Erro ao executar o a2\n");
		exit(EXIT_FAILURE);
	} else {
		waitpid(pid, &status, 0);
		if (WIFEXITED(status)) {
			printf("Processo filho terminou com status: %d\n", WEXITSTATUS(status));
		}
	}
}


void git() {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (pid == 0) {
        // CORREÇÃO: Use apenas uma declaração de args
        char *args[] = {"git", "log", "-1", "--pretty=%B", NULL};
        execvp("git", args);
        perror("Erro ao executar o execvp");
        exit(EXIT_FAILURE);
    } else {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("O processo filho terminou com status: %d\n", WEXITSTATUS(status));
        }
    }
}

char* read_speci_line(int line_number) {
    FILE *quiz_f = fopen("quiz.txt", "r");
    if (quiz_f == NULL) {
        perror("Erro ao abrir o arquivo");
        return NULL;
    }

    char buffer[1024];
    char *result = NULL;
    int linha_atual = 0;

    while (fgets(buffer, sizeof(buffer), quiz_f) != NULL) {
        linha_atual++;
        if (linha_atual == line_number) {
            buffer[strcspn(buffer, "\n")] = '\0'; // Remove o '\n' se presente
            result = strdup(buffer);
            if (result == NULL) {
                fprintf(stderr, "Erro ao alocar memória para a linha\n");
                fclose(quiz_f);
                return NULL;
            }
            break; // Sai do loop após encontrar a linha
        }
    }
    fclose(quiz_f);
    if (result == NULL) {
        fprintf(stderr, "Linha %d não encontrada. Total de linhas no arquivo: %d\n", line_number, linha_atual);
        return NULL;
    }
    return result;
}

//Função para sugerir comandos com base em um texto parcial
void suggest_commands(const char *partial) {
	int found = 0;
	printf("\nSugestão de Comandos:\n");
	for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		if (strncmp(partial, cmds[i].key, strlen(partial)) == 0) {
			printf("  -%s:  %s\n", cmds[i].key, cmds[i].descri);
			found++;
		}
	}
	if (!found) {
		printf("  Nenhum comando encontrado com '%s'. Use 'help' para ver todos os comandos.\n", partial);
	}
	printf("> %s", partial); //reexibe oque o usuario já digitou
	fflush(stdout);
}

//Função para ler uma linha aleatoria
char* read_random_line() {
    int total_lines = contar_linhas();
    if (total_lines <= 0) {
        fprintf(stderr, "Erro: Arquivo vazio ou falha ao contar linhas em '%s'\n", quiz_file);
        return NULL;
    }
    // Gera um número aleatório entre 1 e total_lines
    int random_line = (rand() % total_lines) + 1;
    srand(time(NULL)); 
    // Lê a linha correspondente ao número aleatório
    char* result = read_speci_line(random_line);
    if (result == NULL) {
        fprintf(stderr, "Erro: Falha ao ler a linha %d do arquivo '%s'\n", random_line, quiz_file);
        return NULL;
    }
    return result;
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

// Função para interação com Ollama/2B
void handle_ollama_interaction() {
    char user_prompt[MAX_PROMPT_LEN];
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
    
    snprintf(ollama_full_command, sizeof(ollama_full_command), "ollama run %s \"%s\"", OLLAMA_MODEL, user_prompt);

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
    }
    else {
        if (WIFEXITED(status)) {
            printf("processo do ollama terminou com o status: %d\n", WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Processo ollama terminou com %d\n", WTERMSIG(status));
        }
    }
}

// Função para buscar no google//
void search_google(const char *query) {
    if (query == NULL || query[0] == '\0') {
        printf("Uso: buscar <termo de pesquisa>\n");
        return;
    }

    CURL *curl = curl_easy_init();
    if (curl) {
        // Codifica o texto da busca para ser seguro para uma URL
        // (Ex: "como fazer café" vira "como%20fazer%20café")
        char *encoded_query = curl_easy_escape(curl, query, 0);
        if (encoded_query) {
            char url[1024];
            char command[2048];
            // Monta a URL do Google
            snprintf(url, sizeof(url), "https://www.google.com/search?q=%s", encoded_query);
            
            // Monta o comando para abrir o navegador no Linux
            // As aspas duplas garantem que a URL seja tratada como um único argumento
            snprintf(command, sizeof(command), "xdg-open \"%s\"", url);
            
            printf("Abrindo navegador para buscar por: '%s'\n", query);
            
            // Executa o comando
            system(command);

            // Libera a memória usada pela URL codificada
            curl_free(encoded_query);
        }
        curl_easy_cleanup(curl);
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
    }
}


void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int calculate_sha256(const char *filepath, char *output_hex_string) {
    FILE *file = fopen(filepath, "rb");
    if (!file) {
        perror("Erro ao abrir arquivo");
        return -1;
    }
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256_context;
    SHA256_Init(&sha256_context);
    
    const int bufSize = 4096;
    unsigned char buffer[bufSize];
    size_t bytesRead = 0;
    
    while ((bytesRead = fread(buffer, 1, bufSize, file))) {
        SHA256_Update(&sha256_context, buffer, bytesRead);
    }
    
    SHA256_Final(hash, &sha256_context);
    fclose(file);
    
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output_hex_string + (i * 2), "%02x", hash[i]);
    }
    output_hex_string[64] = 0;
    
    return 0;
}

void handle_hash_command() {
    char choice_str[5];
    int choice;

    disable_raw_mode(); // Desativa o modo raw para entrada padrão

    printf("\n--- Verificador De Hash SHA-256 ---\n");
    printf("1. Gerar hash de um arquivo\n");
    printf("2. Comparar arquivo com hash colado\n");
    printf("3. Sair\n");
    printf("Escolha uma opção: ");

    if (fgets(choice_str, sizeof(choice_str), stdin) == NULL) {
        enable_raw_mode(); // Reativa em caso de erro
        return;
    }
    choice = atoi(choice_str);

    char filepath[256];
    char hash_to_compare[129];
    char calculated_hash[65];

    switch (choice) {
        case 1:
            printf("Digite o caminho do arquivo para gerar hash: ");
            fgets(filepath, sizeof(filepath), stdin);
            filepath[strcspn(filepath, "\n")] = 0;
            if (calculate_sha256(filepath, calculated_hash) == 0) {
                printf("\nHash-256 Gerado:\n%s\n", calculated_hash);
            }
            break;

        case 2:
            printf("Cole o hash-256 para comparar: ");
            if (fgets(hash_to_compare, sizeof(hash_to_compare), stdin) == NULL) break;
            hash_to_compare[strcspn(hash_to_compare, "\n")] = 0;

            printf("Digite o caminho do arquivo para verificar: ");
            if (fgets(filepath, sizeof(filepath), stdin) == NULL) break;
            filepath[strcspn(filepath, "\n")] = 0;

            if (calculate_sha256(filepath, calculated_hash) == 0) {
                printf("\nHash calculado: %s\n", calculated_hash);
                printf("Hash esperado:  %s\n", hash_to_compare);
                if (strncmp(calculated_hash, hash_to_compare, 64) == 0) {
                    printf("\n>>> RESULTADO: Os hashes COINCIDEM!!!\n");
                } else {
                    printf("\n>>> RESULTADO: Os hashes NÃO COINCIDEM!!!\n");
                }
            }
            break;

        case 3:
            printf("Saindo do modo hash...\n");
            break;

        default:
            printf("Opção inválida.\n");
            break;
    }

    enable_raw_mode(); // Reativa o modo raw antes de voltar ao shell
}

int read_command_line(char *buf, int size) {
    int pos = 0; 
    int len = 0; 
    int history_pos = history_count;
    buf[0] = '\0';

    while (1) {
        int c = getchar();

        if (c == EOF || c == 4) { // EOF ou Ctrl+D
            return -1;
        } else if (c == '\n') { // Enter
            printf("\n");
            buf[len] = '\0';
            return len;
        } else if (c == 127 || c == 8) { // Backspace
            if (pos > 0) {
                memmove(&buf[pos - 1], &buf[pos], len - pos);
                pos--;
                len--;
                buf[len] = '\0';
                
                printf("\r> %s\033[K", buf);
                if (pos < len + 2) {
                     printf("\r\x1b[%dC", pos + 2);
                }
                fflush(stdout);
            }
        } else if (c == '\x1b') { // Sequência de escape (setas) 
            int next1 = getchar();
            int next2 = getchar();

            if (next1 == '[') {
                switch(next2) {
                    case 'A': // Seta para Cima
                        if (history_pos > 0) {
                            history_pos--;
                            strncpy(buf, command_history[history_pos], size - 1);
                            len = strlen(buf);
                            pos = len;
                            printf("\r> %s\033[K", buf);
                            fflush(stdout);
                        }
                        break;
                    case 'B': // Seta para Baixo
                        if (history_pos < history_count - 1) {
                            history_pos++;
                            strncpy(buf, command_history[history_pos], size - 1);
                            len = strlen(buf);
                            pos = len;
                            printf("\r> %s\033[K", buf);
                            fflush(stdout);
                        } else {
                            history_pos = history_count;
                            pos = 0;
                            len = 0;
                            buf[0] = '\0';
                            printf("\r> \033[K");
                            fflush(stdout);
                        }
                        break;
                    case 'C': // Seta para Direita
                        if (pos < len) {
                            pos++;
                            printf("\x1b[C");
                            fflush(stdout);
                        }
                        break;
                    case 'D': // Seta para Esquerda
                        if (pos > 0) {
                            pos--;
                            printf("\x1b[D");
                            fflush(stdout);
                        }
                        break;
                }
            }
        } else if (isprint(c) && len < size - 1) {
            memmove(&buf[pos + 1], &buf[pos], len - pos + 1); // +1 para o terminador nulo
            buf[pos] = c;
            len++;
            pos++;
            
            printf("\r> %s\033[K", buf);
            printf("\r\x1b[%dC", pos + 2);
            fflush(stdout);
        }
    }
}

void dispatch(const char *user_in) {
        
        if (user_in[0] == '!') {
        // Pega o comando, ignorando o '!' inicial
        const char *shell_cmd = user_in + 1;
        
        // Antes de executar, restauramos o terminal para o modo normal
        disable_raw_mode();
        printf("Executando no shell: %s\n", shell_cmd);
        
        // Executa o comando
        system(shell_cmd);
        
        // Reativa o nosso modo raw para continuar
        enable_raw_mode();
        
        // Adiciona ao histórico e log
        add_to_history(user_in);
        log_action("Shell Command", shell_cmd);
        return; // Termina a função aqui, não processa como comando interno
    }

    char input_copy[128];
    strncpy(input_copy, user_in, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';

    char *token = strtok(input_copy, " ");
    if (token == NULL) return;
    
    char *args = strtok(NULL, "");

    for (int i = 0; i < alias_count; i++) {
	    if (alias_list[i].name && strcasecmp(token, alias_list[i].name) == 0) {
	        char new_command[256];
	        if (args) {
	            snprintf(new_command, sizeof(new_command), "%s %s", alias_list[i].command, args);
	        } else {
	            snprintf(new_command, sizeof(new_command), "%s", alias_list[i].command);
	        }
	        dispatch(new_command);
	        return;
	    }
    }

    add_to_history(user_in);
    log_action("User Input", user_in);

    for (int i = 0; i < plugin_count; i++) {
        if (strcasecmp(token, loaded_plugins[i].plugin->name) == 0) {
            execute_plugin(token, args);
            return;
        }
    }

    int command_found = 0;
    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
        if (strcasecmp(token, cmds[i].key) == 0) {
            command_found = 1;
            if (strcmp(cmds[i].key, "help") == 0) {
                display_help();
            } else if (strcasecmp(cmds[i].key, "mkdir") == 0) {
                jntd_mkdir(args);
            } else if (strcasecmp(cmds[i].key, "cp") == 0 || strcasecmp(cmds[i].key, "rm") == 0 || strcasecmp(cmds[i].key, "mv") == 0) {
                if (args) {
                    char shell_command[512];
                    snprintf(shell_command, sizeof(shell_command), "%s %s", cmds[i].key, args);
                    disable_raw_mode();
                    printf("Executando: %s\n", shell_command);
                    system(shell_command);
                    enable_raw_mode();
            }  else {
                    printf("Erro, Faltando argumento. Uso %s <argumento>\n", cmds[i].key);
                    }
            } else if (strcasecmp(cmds[i].key, "2b") == 0) {
                handle_ollama_interaction();
            } else if (strcasecmp(cmds[i].key, "cd") == 0) {
                cd(args);
            } else if (strcasecmp(cmds[i].key, "buscar") == 0) {
            	search_google(args);
            } else if (strcasecmp(cmds[i].key, "git") == 0) {
                printf("O repostorio é: https://github.com/Lucasplaygaemes/JNTD\n");
		        printf("Ultimo commit: ");
		        fflush(stdout);
		        git();
            } else if (strcasecmp(cmds[i].key, "his") == 0) {
                display_history();
	        } else if (strcasecmp(token, "alias") == 0) {
		        handle_alias_command(args);
	        } else if (strcasecmp(cmds[i].key, "quiz") == 0) {
		        func_quiz();
	        } else if (strcasecmp(cmds[i].key, "quizale") == 0) {
		        quiz_aleatorio();
	        } else if (strcasecmp(cmds[i].key, "quizt") == 0) {
		        if(args && strcasecmp(args, "cancel") == 0) {
			        cancel_timer("quizt");
		        } else {
			        quiz_timer();
		        }
	        } else if (strcasecmp(cmds[i].key, "rscript") == 0) {
                rscript(args);
	        } else if (strcasecmp(cmds[i].key, "a2") == 0) {
		        a2(args);
	        } else if (strcasecmp(cmds[i].key, "timer") == 0) {
		        if (args && strcasecmp(args, "cancel") == 0) {
			        cancel_timer("timer");
		        } else {
			        timer();
		        }
	        } else if (strcasecmp(cmds[i]. key, "cp_di") == 0) {
		        copy_f_t();
            } else if (strcasecmp(cmds[i].key, "hash") == 0) {
                handle_hash_command();
	        } else if (strcasecmp(cmds[i].key, "download") == 0) {
		        char url[512];
		        char nome[32];

                disable_raw_mode();

		        printf("Qual a URL do arquivo a ser baixado?\n");
		        printf("URL: ");
		        if (fgets(url, sizeof(url), stdin) == NULL) {
                    printf("Erro ou entrada cancelada.\n");
                    enable_raw_mode();
                    return;
                }
                url[strcspn(url, "\n")] = '\0';

		        printf("Qual nome dar ao arquivo?\n");
		        printf("Nome: ");
                if (fgets(nome, sizeof(nome), stdin) == NULL) {
                    printf("Erro ou entrada cancelada.\n");
                    enable_raw_mode();
                    return;
                }
                nome[strcspn(nome, "\n")] = '\0';

                enable_raw_mode();
                if (strlen(url) > 0 && strlen(nome) > 0) {
                    printf("Baixando de '%s' para '%s'...", url, nome);
		    bool dl = download_file(url, nome);
		    printf("Download terminou com status: %d\n", dl);
                } else {
                    printf("URL ou nome do arquivo inválido.\n");
                }
	        } else if (cmds[i].shell_command != NULL) {
                        system(cmds[i].shell_command);
            }
            return;
        }
    }
    
    if (!command_found) {
        printf("Comando '%s' não reconhecido. Use 'help' para ver os comandos disponíveis.\n", token);
	    suggest_commands(token);
    }
}

int main(void) {
    curl_global_init(CURL_GLOBAL_ALL);

    printf("Iniciando o JNTD...\n");
    printf("Bem vindo/a\n");
    //printf("Checado TODOs\n");
    //check_todos();
    enable_raw_mode();

    load_plugins();
    load_aliases_from_file();
    printf("Plugins carregados: %d\n", plugin_count);
    printf("Digite um comando. Use 'help' para ver as opções ou 'sair' para terminar.\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        if (read_command_line(buf, sizeof(buf)) < 0) {
            printf("\n");
            break; 
        }
        if (buf[0] == '\0') {
            continue;
        }
        // Adiciona ao histórico apenas se não for vazio e não for um comando de shell (já é feito no dispatch)
        if (buf[0] != '!') {
            add_to_history(buf);
        }
        if (strcmp(buf, "exit") == 0 || strcmp(buf, ":q") == 0 || strcmp(buf, ":Q") == 0) {
            break;
        }
        dispatch(buf);
    }
    
    for (int i = 0; i < history_count; i++) {
        free(command_history[i]);
    }

    disable_raw_mode();
    printf("\nSaindo....\n");

    curl_global_cleanup();
    return 0;
}
