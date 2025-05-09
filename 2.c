#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
//teste git
//

typedef struct {
	const char *key;
	const char *shell_command;
	const char *descri; // Mantendo 'descri' como você usou
} CmdEntry;

// Corrigindo typos nas descrições
static const CmdEntry cmds[] = {
	{ "listar", "pwd && ls -l", "Diretorio atual e lista arquivos detalhadamente." },
	{ "listart", "pwd && ls -la", "Igual o listar, mas todos os arquivos (incluindo ocultos)." },
	{ "data", "date", "Data e hora atual." },
	{ "quem", "whoami", "Nome do usuário atual." },
	{ "espaco", "df -h .", "Espaço livre do sistema de arquivos atual." },
	{ "sysatt?", "pop-upgrade release check", "Verifica se há atualizações do sistema disponíveis (Pop!_OS)." },
	{ "sudo", "sudo su", "Entra no modo super usuário (USE COM CUIDADO!)." }, // Adicionando aviso
	{ "help", NULL, "Lista todos os comandos disponíveis e suas descrições." },
	{ "criador", "echo lucasplayagemes é o criador deste codigo.", "lucasplayagemes é o criador deste codigo." },
	
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
