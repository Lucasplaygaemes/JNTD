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
    { "todo", NULL, "Adiciona uma ou mais tarefas TODO ao arquivo todo.txt." },
    { "checkt", NULL, "Verifica se há TODOs vencidos ou a vencer hoje." },
    { "listt", NULL, "Lista todas as tarefas TODO salvas no arquivo todo.txt." },
    { "remt", NULL, "Remove uma tarefa TODO do arquivo pelo número." },
    { "editt", NULL, "Edita uma tarefa TODO existente pelo número." },
    { "edit_vim", NULL, "Abre o arquivo todo.txt no vim para edição direta." }
};

// Declaração antecipada das funções

// Comentário que indica que as linhas a seguir são protótipos de funções, ou seja,

// declarações que informam ao compilador sobre a existência e assinatura das funções

// antes de suas implementações completas, permitindo que sejam chamadas em qualquer

// parte do código.

void dispatch(const char *user_in);

// Declara a função 'dispatch', que processa o input do usuário (uma string)

// e direciona a execução para o comando apropriado no JNTD. Recebe como argumento

// uma string constante representando o comando digitado pelo usuário.

void handle_ollama_interaction();

// Declara a função 'handle_ollama_interaction', que gerencia a interação com a IA

// "2B" (provavelmente baseada em Ollama). Não recebe argumentos e é responsável

// por capturar prompts do usuário e processar as respostas da IA.

void handle_calc_interaction(const char *args);

// Declara a função 'handle_calc_interaction', que lida com a calculadora avançada

// integrada ao JNTD. Recebe como argumento uma string constante contendo os

// argumentos ou parâmetros para operações matemáticas (ex.: "soma 5 3").

void display_help();

// Declara a função 'display_help', que exibe uma lista de todos os comandos

// disponíveis no JNTD junto com suas descrições. Não recebe argumentos.

void log_action(const char *action, const char *details);

// Declara a função 'log_action', que registra ações do usuário ou do sistema

// em um arquivo de log. Recebe dois argumentos: a ação realizada (ex.: "User Input")

// e os detalhes da ação (ex.: o comando digitado).

int is_safe_command(const char *cmd);

// Declara a função 'is_safe_command', que verifica se um comando é seguro para

// execução, retornando um inteiro (1 para seguro, 0 para não seguro). Recebe como

// argumento uma string constante representando o comando a ser verificado.

void add_to_history(const char *cmd);

// Declara a função 'add_to_history', que adiciona um comando digitado pelo usuário

// ao histórico de comandos mantido pelo JNTD. Recebe como argumento a string do comando.

void display_history();

// Declara a função 'display_history', que exibe o histórico de comandos digitados

// pelo usuário durante a sessão atual. Não recebe argumentos.

void TODO(const char *input);

// Declara a função 'TODO', que gerencia a adição de tarefas ao sistema de TODOs

// do JNTD. Recebe como argumento uma string constante contendo o input do usuário

// (se houver argumentos adicionais, embora a versão atual seja interativa).

void check_todos();

// Declara a função 'check_todos', que verifica se há tarefas TODO vencidas ou com

// prazo para o dia atual, exibindo alertas ao usuário. Não recebe argumentos.

void cd(const char *args);

// Declara a função 'cd', que altera o diretório atual de trabalho no sistema de

// arquivos. Recebe como argumento uma string constante contendo o caminho do diretório

// de destino (ex.: "/tmp").

int jntd_mkdir(const char *args);

// Declara a função 'jntd_mkdir', que cria um novo diretório no sistema de arquivos.

// Recebe como argumento uma string constante contendo o nome do diretório a ser criado

// e retorna um inteiro (provavelmente para indicar sucesso ou falha).

void rscript(const char *args);

// Declara a função 'rscript', que executa um script de comandos a partir de um arquivo

// texto. Recebe como argumento uma string constante contendo o nome do arquivo de script.


// Função para verificar segurança de comandos

// Comentário que descreve a função 'is_safe_command', cujo propósito é garantir que apenas

// comandos reconhecidos e permitidos sejam executados pelo sistema JNTD.

int is_safe_command(const char *cmd) {

    // Início da implementação da função 'is_safe_command'. Recebe uma string 'cmd' como

    // parâmetro, representando o comando a ser verificado quanto à segurança.

    for (size_t i = 0; i < sizeof(cmds) / sizeof(cmds[0]); ++i) {

        // Loop que percorre todos os comandos definidos na tabela estática 'cmds'. 'sizeof(cmds) / sizeof(cmds[0])'

        // calcula o número total de entradas na tabela, determinando quantas iterações serão feitas.

        if (strcasecmp(cmd, cmds[i].key) == 0) {

            // Compara o comando fornecido ('cmd') com a chave ('key') do comando na tabela 'cmds',

            // ignorando maiúsculas e minúsculas (usando 'strcasecmp'). Se forem iguais, retorna 0.

            return 1; // Comando está na lista de permitidos

            // Se o comando for encontrado na tabela, retorna 1, indicando que é seguro executá-lo.

        }

    }

    return 0; // Comando não reconhecido, não é seguro

    // Se o loop terminar sem encontrar o comando na tabela, retorna 0, indicando que o comando

    // não é reconhecido e, portanto, não é considerado seguro para execução.

}


// Funções para histórico de comandos

// Comentário que introduz as funções relacionadas ao gerenciamento do histórico de comandos

// digitados pelo usuário no JNTD.

void add_to_history(const char *cmd) {

    // Início da implementação da função 'add_to_history', que adiciona um comando ao histórico.

    // Recebe como parâmetro uma string constante 'cmd', representando o comando a ser armazenado.

    if (history_count < MAX_HISTORY) {

        // Verifica se o número atual de comandos no histórico ('history_count') é menor que

        // o limite máximo definido por 'MAX_HISTORY' (provavelmente 50, conforme definição anterior).

        // Se for menor, há espaço para adicionar um novo comando diretamente.

        command_history[history_count] = strdup(cmd);

        // Copia o comando 'cmd' para o array de histórico 'command_history' na posição 'history_count',

        // usando 'strdup' para alocar memória dinamicamente e duplicar a string.

        history_count++;

        // Incrementa o contador 'history_count' para indicar que um novo comando foi adicionado

        // e preparar a próxima posição para um futuro comando.

    } else {

        // Caso contrário, se o histórico já atingiu o limite máximo de comandos, é necessário

        // remover o comando mais antigo (o primeiro) para abrir espaço para o novo comando.

        free(command_history[0]);

        // Libera a memória do primeiro comando no histórico ('command_history[0]'), que será

        // substituído, evitando vazamentos de memória (já que 'strdup' aloca memória dinamicamente).

        for (int i = 1; i < MAX_HISTORY; i++) {

            // Loop que desloca todos os comandos no histórico uma posição para frente, sobrescrevendo

            // o primeiro comando (já liberado) e movendo os outros para abrir espaço no final.

            command_history[i - 1] = command_history[i];

            // Move o comando da posição 'i' para a posição 'i-1', deslocando os comandos para a esquerda.

        }

        command_history[MAX_HISTORY - 1] = strdup(cmd);

        // Adiciona o novo comando na última posição do histórico ('MAX_HISTORY - 1'), novamente

        // usando 'strdup' para alocar memória e copiar a string do comando.

    }

}

void display_history() {
    // Início da implementação da função 'display_history', que exibe o histórico
    // de comandos digitados pelo usuário durante a sessão atual do JNTD. Não recebe
    // argumentos, pois opera diretamente sobre o array global 'command_history'.
    printf("Historico de comandos:\n");
    // Exibe um cabeçalho no terminal indicando que a lista a seguir é o histórico
    // de comandos digitados pelo usuário.
    for (int i = 0; i < history_count; i++) {
        // Loop que percorre o array global 'command_history' desde o índice 0 até
        // 'history_count - 1', ou seja, itera por todos os comandos armazenados no
        // histórico. 'history_count' é uma variável global que registra quantos comandos
        // foram adicionados ao histórico.
        printf("  %d: %s\n", i + 1, command_history[i]);
        // Exibe cada comando do histórico no terminal, formatado com um número sequencial
        // (i + 1, começando de 1 para melhor legibilidade ao usuário) seguido pelo
        // comando armazenado em 'command_history[i]'. O "\n" adiciona uma nova linha
        // após cada comando para separá-los visualmente.
    }
}

void list_todo() {
    // Início da implementação da função 'list_todo', que lista todas as tarefas
    // TODO salvas no arquivo 'todo.txt'. Não recebe argumentos, pois lê diretamente
    // do arquivo no disco.
    printf("Lista de TODOs:\n");
    // Exibe um cabeçalho no terminal indicando que a lista a seguir contém as tarefas
    // TODO armazenadas no sistema.
    printf("------------------------------------------------\n");
    // Exibe uma linha de separação visual com traços, para melhorar a legibilidade
    // e destacar o início da lista de TODOs no terminal.
    FILE *todo_file = fopen("todo.txt", "r");
    // Abre o arquivo 'todo.txt' em modo leitura ("r") e associa o ponteiro 'todo_file'
    // a ele. Este arquivo contém as tarefas TODO salvas anteriormente pelo usuário.
    if (todo_file == NULL) {
        // Verifica se a abertura do arquivo falhou (retorno de 'fopen' é NULL), o que
        // pode acontecer se o arquivo não existir ou se houver problemas de permissão.
        perror("Erro ao abrir o arquivo todo.txt");
        // Exibe uma mensagem de erro detalhada no terminal, explicando o motivo da falha
        // ao abrir o arquivo, usando a função 'perror' que inclui a descrição do erro
        // do sistema (ex.: "No such file or directory").
        printf("Não foi possível listar os TODOs. Arquivo não encontrado ou sem permissão.\n");
        // Exibe uma mensagem adicional ao usuário explicando que a listagem de TODOs
        // não pode ser realizada devido ao erro na abertura do arquivo.
        return;
        // Sai da função imediatamente, encerrando a operação de listagem, pois não é
        // possível prosseguir sem o arquivo aberto.
    }

    char line[1024];
    // Declara um array de caracteres 'line' com tamanho 1024, usado como buffer
    // temporário para armazenar cada linha lida do arquivo 'todo.txt'.
    int count = 0;
    // Declara uma variável inteira 'count' inicializada como 0, que servirá como
    // contador para numerar as linhas (TODOs) exibidas ao usuário.
    while (fgets(line, sizeof(line), todo_file) != NULL) {
        // Loop que lê cada linha do arquivo 'todo.txt' usando a função 'fgets', que
        // armazena a linha lida no buffer 'line' até atingir o tamanho máximo
        // (sizeof(line), ou seja, 1024 caracteres) ou encontrar um caractere de nova
        // linha ('\n'). Continua enquanto 'fgets' não retornar NULL (que indica o fim
        // do arquivo ou um erro).
        line[strcspn(line, "\n")] = '\0'; // Remove nova linha do final
        // Remove o caractere de nova linha ('\n') que 'fgets' inclui na string lida,
        // substituindo-o por um terminador de string ('\0'). 'strcspn(line, "\n")'
        // retorna o índice do primeiro '\n' na string 'line', ou o comprimento da string
        // se não houver '\n'.
        printf("%d: %s\n", ++count, line);
        // Exibe a linha lida no terminal, formatada com um número sequencial
        // ('++count' incrementa o contador antes de exibir, começando de 1) seguido
        // pela string contida em 'line'. O '\n' adiciona uma nova linha após cada TODO
        // para separá-los visualmente.
    }
    fclose(todo_file);
    // Fecha o arquivo 'todo.txt' associado ao ponteiro 'todo_file', liberando os
    // recursos do sistema associados ao arquivo. É uma boa prática fechar arquivos
    // após usá-los para evitar vazamentos de recursos ou bloqueios.
    if (count == 0) {
        // Verifica se nenhuma linha foi lida do arquivo (count ainda é 0), o que indica
        // que o arquivo está vazio ou não contém TODOs.
        printf("Nenhum TODO encontrado no arquivo.\n");
        // Exibe uma mensagem ao usuário informando que não há tarefas TODO para listar.
    }
    printf("------------------------------------------------\n");
    // Exibe uma linha de separação visual com traços, para marcar o fim da lista de
    // TODOs no terminal e melhorar a legibilidade.
}

void remove_todo() {
    list_todo(); // Mostra a lista de TODOs para o usuário escolher
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir o arquivo todo.txt");
        printf("Não foi possível remover TODOs. Arquivo não encontrado ou sem permissão.\n");
        return;
    }

    // Lê todas as linhas para um buffer temporário
    char lines[100][1024]; // Limite de 100 TODOs para simplificar
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

void edit_todo() {
    list_todo(); // Mostra a lista de TODOs para o usuário escolher
    FILE *todo_file = fopen("todo.txt", "r");
    if (todo_file == NULL) {
        perror("Erro ao abrir o arquivo todo.txt");
        printf("Não foi possível editar TODOs. Arquivo não encontrado ou sem permissão.\n");
        return;
    }

    // Lê todas as linhas para um buffer temporário
    char lines[100][1024]; // Limite de 100 TODOs para simplificar
    int count = 0;
    while (fgets(lines[count], sizeof(lines[count]), todo_file) != NULL && count < 100) {
        lines[count][strcspn(lines[count], "\n")] = '\0';
        count++;
    }
    fclose(todo_file);

    if (count == 0) {
        printf("Nenhum TODO para editar.\n");
        return;
    }

    // Solicita o número do TODO a ser editado
    char temp_input[256];
    printf("Digite o número do TODO a ser editado (1-%d, ou 0 para cancelar): ", count);
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

    // Solicita novos valores para o TODO
    char nova_tarefa[512] = {0};
    char novo_usuario[128] = {0};
    char novo_prazo[32] = {0};

    printf("Digite o novo assunto da tarefa (deixe vazio para manter '%s'): ", lines[index-1]);
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler novo assunto");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(nova_tarefa, temp_input, sizeof(nova_tarefa) - 1);
        nova_tarefa[sizeof(nova_tarefa) - 1] = '\0';
        char *ptr = nova_tarefa;
        while (*ptr == ' ') ptr++;
        memmove(nova_tarefa, ptr, strlen(ptr) + 1);
        char *end = nova_tarefa + strlen(nova_tarefa) - 1;
        while (end >= nova_tarefa && *end == ' ') *end-- = '\0';
    } else {
        // Extrai o valor atual (simplificado, pode ser melhorado)
        char *start = strstr(lines[index-1], "TODO: ") + 6;
        char *end = strstr(start, " | Usuário: ");
        if (end) *end = '\0';
        strncpy(nova_tarefa, start, sizeof(nova_tarefa) - 1);
        nova_tarefa[sizeof(nova_tarefa) - 1] = '\0';
    }

    // Solicita novo usuário
    printf("Digite o novo responsável (deixe vazio para manter atual): ");
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler novo responsável");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(novo_usuario, temp_input, sizeof(novo_usuario) - 1);
        novo_usuario[sizeof(novo_usuario) - 1] = '\0';
        char *ptr = novo_usuario;
        while (*ptr == ' ') ptr++;
        memmove(novo_usuario, ptr, strlen(ptr) + 1);
        char *end = novo_usuario + strlen(novo_usuario) - 1;
        while (end >= novo_usuario && *end == ' ') *end-- = '\0';
    } else {
        // Extrai o valor atual (simplificado)
        char *start = strstr(lines[index-1], "Usuário: ") + 9;
        char *end = strstr(start, " | Prazo: ");
        if (end) *end = '\0';
        strncpy(novo_usuario, start, sizeof(novo_usuario) - 1);
        novo_usuario[sizeof(novo_usuario) - 1] = '\0';
    }

    // Solicita nova data
    printf("Digite a nova data de vencimento (dd/mm/aaaa, deixe vazio para manter atual): ");
    fflush(stdout);
    if (fgets(temp_input, sizeof(temp_input), stdin) == NULL) {
        perror("Erro ao ler nova data");
        return;
    }
    temp_input[strcspn(temp_input, "\n")] = '\0';
    if (strlen(temp_input) > 0) {
        strncpy(novo_prazo, temp_input, sizeof(novo_prazo) - 1);
        novo_prazo[sizeof(novo_prazo) - 1] = '\0';
        char *ptr = novo_prazo;
        while (*ptr == ' ') ptr++;
        memmove(novo_prazo, ptr, strlen(ptr) + 1);
        char *end = novo_prazo + strlen(novo_prazo) - 1;
        while (end >= novo_prazo && *end == ' ') *end-- = '\0';
    } else {
        // Extrai o valor atual (simplificado)
        char *start = strstr(lines[index-1], "Prazo: ") + 7;
        strncpy(novo_prazo, start, sizeof(novo_prazo) - 1);
        novo_prazo[sizeof(novo_prazo) - 1] = '\0';
    }

    // Obtém a data de criação original
    char time_str[26] = {0};
    char *start_time = strstr(lines[index-1], "[");
    if (start_time) {
        strncpy(time_str, start_time + 1, 24);
        time_str[24] = '\0';
    } else {
        time_t now = time(NULL);
        ctime_r(&now, time_str);
        time_str[24] = '\0';
    }

    // Escreve todas as linhas, atualizando a escolhida
    FILE *temp_file = fopen("todo_temp.txt", "w");
    if (temp_file == NULL) {
        perror("Erro ao criar arquivo temporário");
        printf("Não foi possível editar o TODO.\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (i == index - 1) {
            fprintf(temp_file, "[%s] TODO: %s | Usuário: %s | Prazo: %s\n", time_str, nova_tarefa, novo_usuario, novo_prazo);
        } else {
            fprintf(temp_file, "%s\n", lines[i]);
        }
    }
    fclose(temp_file);

    // Substitui o arquivo original pelo temporário
    if (remove("todo.txt") != 0 || rename("todo_temp.txt", "todo.txt") != 0) {
        perror("Erro ao atualizar o arquivo todo.txt");
        printf("Não foi possível completar a edição.\n");
        return;
    }
    printf("TODO número %d editado com sucesso.\n", index);
}

void edit_with_vim() {
    printf("Abrindo todo.txt no editor vim para edição...\n");
    int status = system("vim todo.txt");
    if (status == -1) {
        perror("Erro ao executar o comando vim");
        printf("Não foi possível abrir o editor. Verifique se 'vim' está instalado.\n");
    } else if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            printf("Edição concluída com sucesso.\n");
        } else {
            printf("Editor fechado com status %d.\n", exit_code);
        }
    }
}

void TODO(const char *input) {
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
// Função auxiliar para converter data no formato dd/mm/aaaa para time_t

time_t parse_date(const char *date_str) {
    if (strcmp(date_str, "Sem prazo") == 0) {
        return -1; // Indicador de "sem prazo"
    }
    struct tm tm = {0};
    if (sscanf(date_str, "%d/%d/%d", &tm.tm_mday, &tm.tm_mon, &tm.tm_year) != 3) {
        return -1; // Formato inválido
    }
    tm.tm_mon -= 1; // Ajusta mês (0-11 em tm)
    tm.tm_year -= 1900; // Ajusta ano (desde 1900 em tm)
    tm.tm_hour = 23; // Define para o final do dia (23:59:59)
    tm.tm_min = 59;
    tm.tm_sec = 59;
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
            } else if (strcasecmp(cmds[i].key, "todo") == 0) {
		TODO(args);
            } else if (strcasecmp(cmds[i].key, "checkt") == 0) {
		check_todos();
            } else if (strcasecmp(cmds[i].key, "editt") == 0) {
            	edit_todo();
            } else if (strcasecmp(cmds[i].key, "listt") == 0) {
		list_todo();
	    } else if (strcasecmp(cmds[i].key, "remt") == 0) {
		remove_todo(args);
	    }else if (cmds[i].shell_command != NULL) {
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
