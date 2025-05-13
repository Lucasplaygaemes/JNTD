#include <stdio.h>

#define MAX_PROMPT_LEN 256
#define MAX_OLLAMA_CMD (MAX_PROMPT_LEN + 128)
#define OLLAMA_BUFFER_SIZE 1024
#define MODEL_OLLAMA "llama2"
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
//codigo não finalizado do JTND + 2B
void handle_ollama_interaction() {
	char user_prompt[MAX_PROMPT_LEN];
	char ollama_full_command[MAX_OLLAMA_CMD_LEN];
	char ollama_output_line[OLLAMA_BUFFER_SIZE];
	FILE *ollama_pipe;

	printf("Digite o seu prompt para a 2B, a saida sera processada como comandos\n");
	printf("IA prompt> ");
	fflush(stdout);

	if(fgets(user_prompt, sizeof(user_prompt), stdin) == NULL) {
		perror("falha ao ler o promp do usuario para a 2B");
		return;

	}

	user_prompt[strcspn(user_prompt, "\n")] = '\0';

	if(user_prompt[0] == '\0') {
		printf("prompt vazion, nenhuma interação com o ollama");
		return;
	}
	
	snprintf(ollama_full_command, sizeof(ollama_full_command),
			"ollama run %s \"%s\"", OLLAMA_MODEL, user_prompt);

	printf("executando ollama com: %s\n", ollama_full_command);
	printf("aguardando resposta da 2B...\n");
	printf("-------------- 2B output --------------\n");

	ollama_pipe = popen(ollama_full_command, "r");
	if(ollama_pipe == NULL) {
		perror("falha ao executar o comando ollama com popen");
		fprintf(stderr, "verifique se ollama está no path e se a 2B está disponivel\n");
		return;
	}

	while(fgets(ollama_output_line, sizeof(ollama_output_line), ollama_pipe) != NULL) {
		printf("%s", ollama_output_line);
		fflush(stdout);

		ollama_output_line[strcspn(ollama_output_line, "\n")] = '\0';
		if(ollama_output_line[0] != '\0') {
			printf(">>> processando da 2B como comando: '%s'\n", ollama_output_line);
			dispatch(ollama_output_line);
			printf("<<< Fim do processamento da 2B.\n");
		}
	}
	printf("---------------- Fim da fala da 2B --------------");

	int status = pclose(ollama_pipe


	





typedef struct {
	const char *key;
	const char *shell_command;
	const char *descri; // Mantendo 'descri' como você usou
} CmdEntry;

// Corrigindo typos nas descrições
static const CmdEntry cmds[] = {
	{ "ls", "pwd && ls -l", "Diretorio atual e lista arquivos detalhadamente." },
	{ "lsa", "pwd && ls -la", "Igual o listar, mas todos os arquivos (incluindo ocultos)." },
	{ "data", "date", "Data e hora atual." },
	{ "quem", "whoami", "Nome do usuário atual." },
	{ "esp", "df -h .", "Espaço livre do sistema de arquivos atual." },
	{ "sysatt?", "pop-upgrade release check", "Verifica se há atualizações do sistema disponíveis (Pop!_OS)." },
	{ "sudo", "sudo su", "Entra no modo super usuário (USE COM CUIDADO!)." }, // Adicionando aviso
	{ "help", NULL, "Lista todos os comandos disponíveis e suas descrições." },
	{ "criador", "echo lucasplayagemes é o criador deste codigo.", "lucasplayagemes é o criador deste codigo." },
	{ "ia", NULL,
};

void display_help() { // Renomeando para consistência com chamadas anteriores
	printf("Comandos disponíveis:\n");
	for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
		printf("  %-15s: %s\n", cmds[i].key, cmds[i].descri);
	}
}

void dispatch(const char *user_in) {
	for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {
		if (strcmp(user_in, cmds[i].key) == 0) { // Comando encontrado
			if (strcmp(cmds[i].key, "help") == 0) {
				display_help();
			} else if (cmds[i].shell_command != NULL && cmds[i].shell_command[0] != '\0') {
				printf("Executando comando para '%s': %s\n", cmds[i].key, cmds[i].shell_command);
				printf("--------------------------------------------------\n");
				int status = system(cmds[i].shell_command);
				printf("--------------------------------------------------\n");

				if (status == -1) {
					perror("system() falhou ao tentar criar um processo");
				} else {
					// CORREÇÃO DA LÓGICA DE ANINHAMENTO
					if (WIFEXITED(status)) {
						printf("Comando '%s' terminou com codigo de saida %d\n", cmds[i].key, WEXITSTATUS(status));
					} else if (WIFSIGNALED(status)) {
						printf("Comando '%s' terminou por sinal: %d\n", cmds[i].key, WTERMSIG(status));
					} else {
						printf("Comando '%s' terminou com status desconhecido: %d\n", cmds[i].key, status);
					}
				}
			} else {
                // Comando reconhecido mas sem shell_command (e não é 'help')
                printf("Comando '%s' é reconhecido mas não tem uma ação de shell definida.\n", cmds[i].key);
            }
			return; // Sair da função dispatch após encontrar e processar o comando
		}
	}
	// Se o loop terminar, nenhum comando foi encontrado
	printf("Comando invalido: %s. Digite 'help' para ver a lista.\n", user_in);
}

int main(void) {
	char buf[128];

	// Prompt mais genérico ou atualizado
	printf("Digite um comando. Use 'help' para ver as opções ou 'sair' para terminar.\n");

	while (printf("> "), fgets(buf, sizeof(buf), stdin) != NULL) {
		buf[strcspn(buf, "\n")] = '\0'; // Remove newline

		if (strcmp(buf, "sair") == 0) {
			break;
		}

		if (buf[0] == '\0') { // Usuário apenas apertou Enter
			continue;
		}

		// Removida a verificação específica de "apt update" daqui.
		// Se precisar de tratamento especial para "apt update",
		// adicione-o como um CmdEntry e trate em dispatch().

		dispatch(buf);
	}

	printf("Saindo....\n");
	return 0;
}
