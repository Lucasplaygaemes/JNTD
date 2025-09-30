# Makefile Mesclado para todos os projetos JNTD

# --- Configuração do Compilador ---
CC = gcc
# Adicionado -I./a2_files para encontrar os headers do a2
CFLAGS = -g -Wall -Wextra -I. -I./a2_files -I../include
LDFLAGS = -lncursesw -ljansson -lm -lcurl -lpthread -ldl -lssl -lcrypto

# --- Alvos Principais ---
TARGETS = a2 2bt

# --- Diretórios ---
A2_DIR = a2_files

# --- Plugins ---
PLUGIN_CFLAGS = -fPIC -shared
# Corrigido o caminho para a pasta de plugins
PLUGIN_SRCS = $(wildcard plugins/*.c)
PLUGIN_TARGETS = $(PLUGIN_SRCS:.c=.so)

# O alvo padrão 'all' compila os executáveis e os plugins
all: $(TARGETS) $(PLUGIN_TARGETS)


# --- Regras de Compilação para o Editor 'a2' ---

# Arquivos fonte para o a2 (adicionando defs.c que estava faltando)
A2_SOURCES = a2.c command_execution.c defs.c direct_navigation.c fileio.c lsp_client.c others.c screen_ui.c window_managment.c timer.c
# Adiciona o prefixo do diretório para os fontes e objetos
A2_SRCS = $(addprefix $(A2_DIR)/, $(A2_SOURCES))
A2_OBJS = $(A2_SRCS:.c=.o)

# Regra para linkar o executável a2 (na raiz do projeto)
a2: $(A2_OBJS)
	$(CC) $(CFLAGS) -o $@ $(A2_OBJS) $(LDFLAGS)

# Regra de padrão para compilar os arquivos .c do a2 em .o no diretório de objetos
$(A2_DIR)/%.o: $(A2_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@


# --- Regras de Compilação para '2bt' e Plugins ---

# Regra para compilar o 2bt
2bt: 2bt.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Regra de padrão para compilar os plugins
plugins/%.so: plugins/%.c
	$(CC) $(CFLAGS) $(PLUGIN_CFLAGS) -o $@ $<


# --- Alvos de Limpeza e Utilitários ---

# Limpa todos os arquivos gerados por este Makefile
clean:
	rm -f $(TARGETS) $(A2_OBJS) $(PLUGIN_TARGETS)

# Gera o compile_commands.json (apenas para o projeto a2)
compile_commands:
	@echo "Para gerar compile_commands.json para o editor a2, execute: bear -- make a2"

.PHONY: all clean compile_commands a2 2bt plugins
