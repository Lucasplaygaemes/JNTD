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
#include <jansson.h>
#include <strings.h>
#include <string.h>


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


// ===================================================================
// Definições para suporte a LSP (Language Server Protocol)
// ===================================================================

// Estrutura para representar uma posição no documento (LSP)
typedef struct {
    int line;
    int character;
} LspPosition;

// Estrutura para representar um intervalo no documento (LSP)
typedef struct {
    LspPosition start;
    LspPosition end;
} LspRange;

// Estrutura para representar uma mensagem de diagnóstico do LSP
typedef struct {
    LspRange range;
    int severity; // 1=Error, 2=Warning, 3=Info, 4=Hint
    char *message;
    char *code;
} LspDiagnostic;

// Estrutura para representar uma sugestão de completion do LSP
typedef struct {
    char *label;
    char *detail;
    char *documentation;
    char *insertText;
    int kind; // LSP CompletionItemKind
} LspCompletionItem;

// Estrutura para representar um símbolo (LSP)
typedef struct {
    char *name;
    int kind; // LSP SymbolKind
    LspRange range;
    LspRange selectionRange;
    char *containerName;
} LspSymbolInformation;

// Estrutura para o cliente LSP
typedef struct {
    pid_t server_pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    bool initialized;
    char *rootUri;
    char *workspaceFolders;
    char *languageId;
    char *compilerFlags;
    char *compilationDatabase;
} LspClient;


// Estrutura para armazenar o estado do LSP para um arquivo
typedef struct {
    char *uri;
    int version;
    LspDiagnostic *diagnostics;
    int diagnostics_count;
    bool needs_update;
} LspDocumentState;



// --- Definições do Editor ---
#define MAX_LINES 16486
#define MAX_LINE_LEN 4096
#define STATUS_MSG_LEN 250
#define PAGE_JUMP 10
#define TAB_SIZE 4
#define MAX_JANELAS 15 
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




// Constantes para LSP
#define LSP_SEVERITY_ERROR 1
#define LSP_SEVERITY_WARNING 2
#define LSP_SEVERITY_INFO 3
#define LSP_SEVERITY_HINT 4


// Completion Item Kinds (LSP Specification 3.17)
#define LSP_COMPLETION_KIND_TEXT 1
#define LSP_COMPLETION_KIND_METHOD 2
#define LSP_COMPLETION_KIND_FUNCTION 3
#define LSP_COMPLETION_KIND_CONSTRUCTOR 4
#define LSP_COMPLETION_KIND_FIELD 5
#define LSP_COMPLETION_KIND_VARIABLE 6
#define LSP_COMPLETION_KIND_CLASS 7
#define LSP_COMPLETION_KIND_INTERFACE 8
#define LSP_COMPLETION_KIND_MODULE 9
#define LSP_COMPLETION_KIND_PROPERTY 10
#define LSP_COMPLETION_KIND_UNIT 11
#define LSP_COMPLETION_KIND_VALUE 12
#define LSP_COMPLETION_KIND_ENUM 13
#define LSP_COMPLETION_KIND_KEYWORD 14
#define LSP_COMPLETION_KIND_SNIPPET 15
#define LSP_COMPLETION_KIND_COLOR 16
#define LSP_COMPLETION_KIND_FILE 17
#define LSP_COMPLETION_KIND_REFERENCE 18
#define LSP_COMPLETION_KIND_FOLDER 19
#define LSP_COMPLETION_KIND_ENUM_MEMBER 20
#define LSP_COMPLETION_KIND_CONSTANT 21
#define LSP_COMPLETION_KIND_STRUCT 22
#define LSP_COMPLETION_KIND_EVENT 23
#define LSP_COMPLETION_KIND_OPERATOR 24
#define LSP_COMPLETION_KIND_TYPE_PARAMETER 25

// Symbol Kinds (LSP Specification 3.17)
#define LSP_SYMBOL_KIND_FILE 1
#define LSP_SYMBOL_KIND_MODULE 2
#define LSP_SYMBOL_KIND_NAMESPACE 3
#define LSP_SYMBOL_KIND_PACKAGE 4
#define LSP_SYMBOL_KIND_CLASS 5
#define LSP_SYMBOL_KIND_METHOD 6
#define LSP_SYMBOL_KIND_PROPERTY 7
#define LSP_SYMBOL_KIND_FIELD 8
#define LSP_SYMBOL_KIND_CONSTRUCTOR 9
#define LSP_SYMBOL_KIND_ENUM 10
#define LSP_SYMBOL_KIND_INTERFACE 11
#define LSP_SYMBOL_KIND_FUNCTION 12
#define LSP_SYMBOL_KIND_VARIABLE 13
#define LSP_SYMBOL_KIND_CONSTANT 14
#define LSP_SYMBOL_KIND_STRING 15
#define LSP_SYMBOL_KIND_NUMBER 16
#define LSP_SYMBOL_KIND_BOOLEAN 17
#define LSP_SYMBOL_KIND_ARRAY 18
#define LSP_SYMBOL_KIND_OBJECT 19
#define LSP_SYMBOL_KIND_KEY 20
#define LSP_SYMBOL_KIND_NULL 21
#define LSP_SYMBOL_KIND_ENUM_MEMBER 22
#define LSP_SYMBOL_KIND_STRUCT 23
#define LSP_SYMBOL_KIND_EVENT 24
#define LSP_SYMBOL_KIND_OPERATOR 25
#define LSP_SYMBOL_KIND_TYPE_PARAMETER 26

// Estrutura para representar uma mensagem LSP
typedef struct {
    char *jsonrpc;
    char *id;
    char *method;
    json_t *params;
    json_t *result;
    json_t *error;
} LspMessage;

// Função para criar uma mensagem LSP
LspMessage* lsp_create_message();
// Função para liberar uma mensagem LSP
void lsp_free_message(LspMessage *msg);
// Função para parsear uma mensagem JSON em LspMessage
LspMessage* lsp_parse_message(const char *json_str);
// Função para serializar uma LspMessage para JSON
char* lsp_serialize_message(LspMessage *msg);



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
    //EditorState state;
    LspClient *lsp_client;
    LspDocumentState *lsp_document;
    bool lsp_enabled;
    
    
    double last_change_time;
    time_t lsp_init_time;
    int lsp_init_retries;
 
    // Para tooltip de erro
    double hover_start_time;
    int hover_line;
    int hover_col;
    bool hover_showing;
    char hover_message[STATUS_MSG_LEN];
    WINDOW *hover_win;
    LspDiagnostic *current_hover_diag;
    
    // Para debouncing e atualização
    bool force_redraw;
};


// Declarations to manage file type
typedef struct {
    const char *extension;
    const char *syntax_file;
} ExtensionSyntaxMap;

const char * get_syntax_file_from_extension(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "syntaxes/c.syntax"; // Padrão para C
    
    if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0)
        return "syntaxes/c.syntax";
    else if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".hpp") == 0)
        return "syntaxes/cpp.syntax";
    else if (strcmp(ext, ".py") == 0)
        return "syntaxes/python.syntax";
    else if (strcmp(ext, ".php") == 0)
        return "syntaxes/php.syntax";
    else if (strcmp(ext, ".js") == 0)
        return "syntaxes/javascript.syntax";
    else if (strcmp(ext, ".java") == 0)
        return "syntaxes/java.syntax";
    else if (strcmp(ext, ".ts") == 0)
        return "syntaxes/typescript.syntax";
    else if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "syntaxes/html.syntax";
    else if (strcmp(ext, ".css") == 0)
        return "syntaxes/css.syntax";
    else if (strcmp(ext, ".rb") == 0)
        return "syntaxes/ruby.syntax";
    else if (strcmp(ext, ".rs") == 0)
        return "syntaxes/rust.syntax";
    else if (strcmp(ext, ".go") == 0)
        return "syntaxes/go.syntax";
    
    return "syntaxes/c.syntax"; // Padrão
}




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

// Protótipos de funções para LSP
void lsp_initialize(EditorState *state);
void lsp_shutdown(EditorState *state);
void lsp_did_open(EditorState *state);
void lsp_did_change(EditorState *state);
void lsp_did_save(EditorState *state);
void lsp_did_close(EditorState *state);
void lsp_request_completion(EditorState *state, int line, int col);
void lsp_goto_definition(EditorState *state);
void lsp_goto_references(EditorState *state);
void lsp_format_document(EditorState *state);
void lsp_rename_symbol(EditorState *state, const char *new_name);
void lsp_process_messages(EditorState *state);
bool lsp_is_available(EditorState *state);
char* lsp_get_uri_from_path(const char *path);
void lsp_parse_diagnostics(EditorState *state, const char *json_response);
void lsp_parse_completion(EditorState *state, const char *json_response);
void lsp_draw_diagnostics(WINDOW *win, EditorState *state);
void lsp_cleanup_diagnostics(EditorState *state);
char* json_escape_string(const char *str);

// Adicione estas declarações na seção de protótipos de funções LSP
void lsp_init_document_state(EditorState *state);
void lsp_free_document_state(EditorState *state);
char* lsp_get_uri_from_path(const char *path);
void lsp_send_initialize(EditorState *state);
void lsp_send_did_change(EditorState *state);
void process_lsp_status(EditorState *state);
void process_lsp_symbols(EditorState *state);
void process_lsp_hover(EditorState *state);
void lsp_log(const char *format, ...);
void lsp_request_diagnostics(EditorState *state);
void lsp_force_diagnostics(EditorState *state);
void lsp_send_message(EditorState *state, const char *json_message);
void lsp_process_received_data(EditorState *state, const char *buffer, size_t buffer_len);
char* json_escape_string(const char *str);

void lsp_send_did_change(EditorState *state);
void lsp_force_diagnostics(EditorState *state);




void create_hover_window(EditorState *state);
void destroy_hover_window(EditorState *state);
void draw_hover_tooltip(WINDOW *win, EditorState *state);
