#define MAX_UNDO_LEVELS 512
#define _XOPEN_SOURCE 700
#define NCURSES_WIDECHAR 1


#include "timer.h"
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <wctype.h>
#include <errno.h>
#include <locale.h>
#include <wchar.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <limits.h>

typedef struct EditorState EditorState;

typedef struct {
    EditorState *estado; // O estado do editor para esta janela (buffer, cursor, etc.)
    WINDOW *win;         // A janela ncurses associada
    int y, x, altura, largura; // Posição e dimensões na tela
} JanelaEditor;

// Gerencia todas as janelas ativas
typedef struct {
    JanelaEditor **janelas;
    int num_janelas;
    int janela_ativa_idx;
} GerenciadorJanelas;

// Variável global para o gerenciador
GerenciadorJanelas gerenciador;


// --- Definições do Editor ---
#define MAX_LINES 16486
#define MAX_LINE_LEN 4096
#define STATUS_MSG_LEN 250
#define PAGE_JUMP 10
#define TAB_SIZE 4
#define MAX_COMMAND_HISTORY 50 
#define AUTO_SAVE_INTERVAL 1 // Segundos
#define AUTO_SAVE_EXTENSION ".sv"

#define KEY_CTRL_P 16
#define KEY_CTRL_DEL 520
#define KEY_CTRL_K 11
#define KEY_CTRL_F 6
#define KEY_CTRL_D 4
#define KEY_CTRL_A 1
#define KEY_CTRL_G 7
#define KEY_CTRL_RIGHT_BRACKET 29
#define KEY_CTRL_LEFT_BRACKET 27

typedef struct {
    char **lines;
    int num_lines;
    int current_line;
    int current_col;
    int ideal_col;
    int top_line;
    int left_col;
} EditorSnapshot;

// Função para o directory manager
typedef struct {
    char *path;
    int access_count;
} DirectoryInfo;

// Enum para os tipos de sintaxe que definimos no arquivo .syntax
typedef enum {
    SYNTAX_KEYWORD,
    SYNTAX_TYPE,
    SYNTAX_STD_FUNCTION
} SyntaxRuleType;

// Estrutura para guardar uma regra de sintaxe (uma palavra e seu tipo)
typedef struct {
    char *word;
    SyntaxRuleType type;
} SyntaxRule;

// --- Estrutura para a tela de ajuda ---
typedef struct {
    const char *command;
    const char *description;
} CommandInfo;

// --- Estrutura para o visualizador de arquivos ---
typedef struct {
    char **lines;
    int num_lines;
} FileViewer;

// --- Dicionários estáticos para autocompletar headers (será substituído no futuro) ---
const char* stdio_h_symbols[] = {"printf", "scanf", "fprintf", "sprintf", "sscanf", "fopen", "fclose", "fgetc", "fgets", "fputc", "fputs", "fread", "fwrite", "fseek", "ftell", "rewind", "remove", "rename", "tmpfile", "tmpnam", "setvbuf", "setbuf", "ferror", "feof", "clearerr", "perror", "FILE", "EOF", "NULL", "SEEK_SET", "SEEK_CUR", "SEEK_END"};
const char* stdlib_h_symbols[] = {"malloc", "calloc", "realloc", "free", "atoi", "atof", "atol", "strtod", "strtol", "strtoul", "rand", "srand", "system", "exit", "getenv", "abs", "labs", "div", "ldiv", "qsort", "bsearch", "EXIT_SUCCESS", "EXIT_FAILURE", "RAND_MAX"};
const char* string_h_symbols[] = {"strcpy", "strncpy", "strcat", "strncat", "strcmp", "strncmp", "strchr", "strrchr", "strlen", "strspn", "strcspn", "strpbrk", "strstr", "strtok", "memset", "memcpy", "memmove", "memcmp", "memchr", "strerror"};

typedef struct {
    const char* header_name;
    const char** symbols;
    int count;
} HeaderDef;

HeaderDef known_headers[] = {
    {"<stdio.h>", stdio_h_symbols, sizeof(stdio_h_symbols) / sizeof(char*)},
    {"<stdlib.h>", stdlib_h_symbols, sizeof(stdlib_h_symbols) / sizeof(char*)},
    {"<string.h>", string_h_symbols, sizeof(string_h_symbols) / sizeof(char*)}
};

int num_known_headers = sizeof(known_headers) / sizeof(HeaderDef);

const char* editor_commands[] = {
    "w", "q", "wq", "open", "new", "help", "gcc", "rc"
};
int num_editor_commands = sizeof(editor_commands) / sizeof(char*);

typedef enum { COMPLETION_NONE, COMPLETION_TEXT, COMPLETION_COMMAND, COMPLETION_FILE } CompletionMode;

typedef enum { NORMAL, INSERT, COMMAND } EditorMode;

typedef enum {
    RECOVER_FROM_SV,
    RECOVER_OPEN_ORIGINAL,
    RECOVER_DIFF,
    RECOVER_IGNORE,
    RECOVER_ABORT
} FileRecoveryChoice;

// Estrutura para guardar a posição de um bracket
typedef struct {
    int line;
    int col;
    char type;
} BracketInfo;

// Estrutura auxiliar para a pilha de análise de brackets
typedef struct {
    int line;
    int col;
    char type;
} BracketStackItem;

struct EditorState {
    char *lines[MAX_LINES];
    int num_lines, current_line, current_col, ideal_col, top_line, left_col, command_pos;
    EditorMode mode;
    char filename[256], status_msg[STATUS_MSG_LEN], command_buffer[100];
    char *command_history[MAX_COMMAND_HISTORY];
    int history_count;
    int history_pos;
    // Campos para o estado do autocompletar
    CompletionMode completion_mode;
    char **completion_suggestions;
    int num_suggestions;
    int selected_suggestion;
    WINDOW *completion_win;
    char word_to_complete[100];
    int completion_start_col;
    int completion_scroll_top;
    
    // Campos para sintaxe dinâmica
    SyntaxRule *syntax_rules;
    int num_syntax_rules;
    char **dictionary_words;
    int num_dictionary_words;
    
    char last_search[100];
    int last_match_line;
    int last_match_col;

    bool buffer_modified;
    time_t last_file_mod_time;
    
    EditorSnapshot *undo_stack[MAX_UNDO_LEVELS];
    int undo_count;
    EditorSnapshot *redo_stack[MAX_UNDO_LEVELS];
    int redo_count;
    time_t last_auto_save_time;
    bool auto_indent_on_newline;
    bool paste_mode;
    bool word_wrap_enabled;

    DirectoryInfo **recent_dirs;
    int num_recent_dirs;

    // Campos para destaque de brackets
    BracketInfo *unmatched_brackets;
    int num_unmatched_brackets;
};

// --- Declarações de Funções ---

// Funções de gerenciamento de janelas
void inicializar_gerenciador_janelas();
void criar_nova_janela(const char *filename);
void fechar_janela_ativa(bool *should_exit);
void proxima_janela();
void janela_anterior();
void recalcular_layout_janelas();
void redesenhar_todas_as_janelas();
void posicionar_cursor_ativo();
void free_janela_editor(JanelaEditor* jw);
void free_editor_state(EditorState* state);

void inicializar_ncurses();
void display_help_screen();
void display_output_screen(const char *title, const char *filename);
void compile_file(EditorState *state, char* args);
void execute_shell_command(EditorState *state);
void add_to_command_history(EditorState *state, const char* command);
void editor_redraw(WINDOW *win, EditorState *state);
void load_file(EditorState *state, const char *filename);
void save_file(EditorState *state);
void editor_handle_enter(EditorState *state);
void editor_handle_backspace(EditorState *state);
void editor_insert_char(EditorState *state, wint_t ch);
void editor_delete_line(EditorState *state);
void editor_move_to_next_word(EditorState *state);
void editor_move_to_previous_word(EditorState *state);
void process_command(EditorState *state, bool *should_exit);
void ensure_cursor_in_bounds(EditorState *state);
void adjust_viewport(WINDOW *win, EditorState *state);
void handle_insert_mode_key(EditorState *state, wint_t ch);
void handle_command_mode_key(EditorState *state, wint_t ch, bool *should_exit);
void load_syntax_file(EditorState *state, const char *filename);
FileViewer* create_file_viewer(const char* filename);
void destroy_file_viewer(FileViewer* viewer);
void editor_find(EditorState *state);
void search_google(const char *query);
void reload_file(EditorState *state);
void check_external_modification(EditorState *state);
void get_visual_pos(WINDOW *win, EditorState *state, int *visual_y, int *visual_x);
void load_directory_history(EditorState *state);
void save_directory_history(EditorState *state);
void update_directory_access(EditorState *state, const char *path);
void display_directory_navigator(EditorState *state);
void prompt_for_directory_change(EditorState *state);
void push_undo(EditorState *state);
void auto_save(EditorState *state);
void do_undo(EditorState *state);
void do_redo(EditorState *state);
void editor_find_next(EditorState *state);
void editor_find_previous(EditorState *state);
void free_snapshot(EditorSnapshot *snapshot);
time_t get_file_mod_time(const char *filename);
char* trim_whitespace(char *str);
void diff_command(EditorState *state, const char *args);
int get_visual_col(const char *line, int byte_col);
void clear_redo_stack(EditorState *state);
void editor_find_unmatched_brackets(EditorState *state);
bool is_unmatched_bracket(EditorState *state, int line, int col);


// Funções para recuperação de arquivo
FileRecoveryChoice display_recovery_prompt(WINDOW *win, EditorState *state);
void handle_file_recovery(EditorState *state, const char *original_filename, const char *sv_filename);
void run_and_display_command(const char* command, const char* title);
void load_file_core(EditorState *state, const char *filename);

// Declarações de Funções do Autocompletar
void editor_start_file_completion(EditorState *state);
void editor_start_command_completion(EditorState *state);
void editor_start_completion(EditorState *state);
void editor_end_completion(EditorState *state);
void editor_draw_completion_win(WINDOW *win, EditorState *state);
void editor_apply_completion(EditorState *state);
void add_suggestion(EditorState *state, const char *suggestion);
void save_last_line(const char *filename, int line);
int load_last_line(const char *filename);

