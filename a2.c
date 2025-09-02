#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

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

// --- Novas Estruturas para Multijanela ---
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

// --- Fim das Novas Estruturas ---

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

// --- Definições das Funções ---

// ===================================================================
// 1. Core Editor & Initialization
// ===================================================================

void inicializar_ncurses() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_BLACK, COLOR_BLUE); init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK); init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(8, COLOR_WHITE, COLOR_BLACK);
    init_pair(9, COLOR_BLACK, COLOR_MAGENTA);
    init_pair(10, COLOR_GREEN, COLOR_BLACK);
    init_pair(11, COLOR_RED, COLOR_BLACK);
    bkgd(COLOR_PAIR(8));
}

int main(int argc, char *argv[]) {
    start_work_timer();
    setlocale(LC_ALL, ""); 
    inicializar_ncurses();
    
    inicializar_gerenciador_janelas();

    if (argc > 1) {
        criar_nova_janela(argv[1]);
        EditorState *state = gerenciador.janelas[0]->estado;
        if (argc > 2) {
            state->current_line = atoi(argv[2]) - 1;
            if (state->current_line >= state->num_lines) {
                state->current_line = state->num_lines - 1;
            }
            if (state->current_line < 0) {
                state->current_line = 0;
            }
            state->ideal_col = 0;
        } else {
            state->current_line = load_last_line(state->filename);
             if (state->current_line >= state->num_lines) {
                state->current_line = state->num_lines > 0 ? state->num_lines - 1 : 0;
            }
            if (state->current_line < 0) {
                state->current_line = 0;
            }
        }
    } else {
        criar_nova_janela(NULL);
    }

    bool should_exit = false;
    while (!should_exit) {
        if (gerenciador.num_janelas == 0) {
            should_exit = true;
            continue;
        }

        EditorState *state = gerenciador.janelas[gerenciador.janela_ativa_idx]->estado;
        WINDOW *active_win = gerenciador.janelas[gerenciador.janela_ativa_idx]->win;

        ensure_cursor_in_bounds(state);
        check_external_modification(state);
        
        time_t now = time(NULL);
        if (now - state->last_auto_save_time >= AUTO_SAVE_INTERVAL) {
            auto_save(state);
            state->last_auto_save_time = now;
        }
        
        redesenhar_todas_as_janelas();
        wint_t ch;
        wget_wch(active_win, &ch);

        if (state->completion_mode != COMPLETION_NONE) {
            int win_h = 0;
            if (state->completion_win) win_h = getmaxy(state->completion_win);

            switch(ch) {
                case KEY_UP:
                    state->selected_suggestion--;
                    if (state->selected_suggestion < 0) {
                        state->selected_suggestion = state->num_suggestions - 1;
                        if(win_h > 0) {
                           int new_top = state->num_suggestions - win_h;
                           state->completion_scroll_top = new_top > 0 ? new_top : 0;
                        }
                    }
                    if (state->selected_suggestion < state->completion_scroll_top) {
                        state->completion_scroll_top = state->selected_suggestion;
                    }
                    break;
                case ' 	':
                case KEY_DOWN:
                    state->selected_suggestion++;
                    if (state->selected_suggestion >= state->num_suggestions) {
                        state->selected_suggestion = 0;
                        state->completion_scroll_top = 0;
                    }
                    if (win_h > 0 && state->selected_suggestion >= state->completion_scroll_top + win_h) {
                        state->completion_scroll_top = state->selected_suggestion - win_h + 1;
                    }
                    break;
                case KEY_ENTER: case '\n':
                    editor_apply_completion(state);
                    break;
                case 27: // ESC
                    editor_end_completion(state);
                    break;
                default: 
                    editor_end_completion(state);
                    if (state->mode == INSERT) {
                        handle_insert_mode_key(state, ch);
                    } else if (state->mode == COMMAND) {
                        handle_command_mode_key(state, ch, &should_exit);
                    }
                    break;
            }
            continue; 
        }
            
        if (ch == 27) {
            nodelay(active_win, TRUE);
            int next_ch = wgetch(active_win);
            nodelay(active_win, FALSE);

            if (next_ch == ERR) { // Just a single ESC press
                 if (state->mode == INSERT) {
                    state->mode = NORMAL;
                }
            } else { // Alt key sequence
                if (next_ch == '[') { // This is Alt+[ for previous window
                    janela_anterior();
                } else if (next_ch == ']') { // This is Alt+] for next window
                    proxima_janela();
                } else if (next_ch == 'x' || next_ch == 'X') { // Alt+X to close window
                     fechar_janela_ativa(&should_exit);
                } else if (next_ch == '\n' || next_ch == KEY_ENTER) { // Alt+Enter to create new window
                    criar_nova_janela(NULL);
                } else if (next_ch == 'z' || next_ch == 'Z') {
                    do_undo(state);
                } else if (next_ch == 'y' || next_ch == 'Y') {
                    do_redo(state);
                } else if (next_ch == 'f' || next_ch == 'w') {
                    editor_move_to_next_word(state);
                } else if (next_ch == 'b' || next_ch == 'q') {
                    editor_move_to_previous_word(state);
                } else if (next_ch == 'g' || next_ch == 'G') {
                    prompt_for_directory_change(state);
                }
            }
            continue;
        }

        switch (state->mode) {
            case NORMAL:
                switch (ch) {
                    case 'i': state->mode = INSERT; break;
                    case ':': state->mode = COMMAND; state->history_pos = state->history_count; state->command_buffer[0] = '\0'; state->command_pos = 0; break;
                    case KEY_CTRL_RIGHT_BRACKET: proxima_janela(); break;
                    case KEY_CTRL_LEFT_BRACKET: janela_anterior(); break;
                    case KEY_CTRL_F: editor_find(state); break;
                    case KEY_CTRL_DEL: editor_delete_line(state); break;
                    case KEY_CTRL_K: editor_delete_line(state); break;
                    case KEY_CTRL_D: editor_find_next(state); break;
                    case KEY_CTRL_A: editor_find_previous(state); break;
                    case KEY_CTRL_G: display_directory_navigator(state); break;
                    case KEY_UP:
                        if (state->word_wrap_enabled) {
                            if (state->current_line > 0) {
                                state->current_line--;
                                state->current_col = state->ideal_col;
                            }
                            break;
                        }
                    case KEY_DOWN: {
                        if (state->current_line < state->num_lines - 1) {
                            state->current_line++;
                            state->current_col = state->ideal_col;
                        }
                        break;                        
                    }
                    case KEY_LEFT:
                        if (state->current_col > 0) {
                            state->current_col--;
                            while (state->current_col > 0 && (state->lines[state->current_line][state->current_col] & 0xC0) == 0x80) {
                                state->current_col--;
                            }
                        }
                        state->ideal_col = state->current_col;
                        break;
                    case KEY_RIGHT: {
                        char* line = state->lines[state->current_line];
                        if (line && state->current_col < strlen(line)) {
                            state->current_col++;
                            while (line[state->current_col] != '\0' && (line[state->current_col] & 0xC0) == 0x80) {
                                state->current_col++;
                            }
                        }
                        state->ideal_col = state->current_col;
                        } break;
                    case KEY_PPAGE: case KEY_SR: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line > 0) state->current_line--; state->current_col = state->ideal_col; break;
                    case KEY_NPAGE: case KEY_SF: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line < state->num_lines - 1) state->current_line++; state->current_col = state->ideal_col; break;
                    case KEY_HOME: state->current_col = 0; state->ideal_col = 0; break;
                    case KEY_END: { char* line = state->lines[state->current_line]; if(line) state->current_col = strlen(line); state->ideal_col = state->current_col; } break;
                    case KEY_SDC: editor_delete_line(state); break;
                }
                break;
            case INSERT:
                handle_insert_mode_key(state, ch);
                break;
            case COMMAND:
                handle_command_mode_key(state, ch, &should_exit);
                break;
        }
    }

    for (int i = 0; i < gerenciador.num_janelas; i++) {
        free_janela_editor(gerenciador.janelas[i]);
    }
    free(gerenciador.janelas);

    stop_and_log_work();
    endwin(); 
    return 0;
}

// ===================================================================
// Window Management
// ===================================================================

void free_editor_state(EditorState* state) {
    if (!state) return;

    if (state->filename[0] != '[') {
        save_last_line(state->filename, state->current_line);
    }
    if (state->completion_mode != COMPLETION_NONE) editor_end_completion(state);
    for(int j=0; j < state->history_count; j++) free(state->command_history[j]);
    for (int j = 0; j < state->undo_count; j++) free_snapshot(state->undo_stack[j]);
    for (int j = 0; j < state->redo_count; j++) free_snapshot(state->redo_stack[j]);
    for (int j = 0; j < state->num_syntax_rules; j++) free(state->syntax_rules[j].word);
    free(state->syntax_rules);
    for (int j = 0; j < state->num_recent_dirs; j++) {
        free(state->recent_dirs[j]->path);
        free(state->recent_dirs[j]);
    }
    free(state->recent_dirs);
    for (int j = 0; j < state->num_lines; j++) {
        if (state->lines[j]) free(state->lines[j]);
    }
    free(state);
}

void free_janela_editor(JanelaEditor* jw) {
    if (!jw) return;
    free_editor_state(jw->estado);
    delwin(jw->win);
    free(jw);
}

void inicializar_gerenciador_janelas() {
    gerenciador.janelas = NULL;
    gerenciador.num_janelas = 0;
    gerenciador.janela_ativa_idx = -1;
}

void recalcular_layout_janelas() {
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    if (gerenciador.num_janelas == 0) return;

    int largura_janela = screen_cols / gerenciador.num_janelas;
    for (int i = 0; i < gerenciador.num_janelas; i++) {
        JanelaEditor *jw = gerenciador.janelas[i];
        jw->y = 0;
        jw->x = i * largura_janela;
        jw->altura = screen_rows;
        jw->largura = (i == gerenciador.num_janelas - 1) ? (screen_cols - jw->x) : largura_janela;

        if (jw->win) {
            delwin(jw->win);
        }
        jw->win = newwin(jw->altura, jw->largura, jw->y, jw->x);
        keypad(jw->win, TRUE);
        scrollok(jw->win, FALSE);
    }
}

void criar_nova_janela(const char *filename) {
    gerenciador.num_janelas++;
    gerenciador.janelas = realloc(gerenciador.janelas, sizeof(JanelaEditor*) * gerenciador.num_janelas);

    JanelaEditor *nova_janela = calloc(1, sizeof(JanelaEditor));
    nova_janela->estado = calloc(1, sizeof(EditorState));
    
    EditorState *state = nova_janela->estado;
    strcpy(state->filename, "[No Name]");
    state->mode = NORMAL; 
    state->completion_mode = COMPLETION_NONE;
    state->unmatched_brackets = NULL;
    state->num_unmatched_brackets = 0;
    state->last_search[0] = '\0';
    state->last_match_line = -1;
    state->last_match_col = -1;
    state->buffer_modified = false;
    state->last_file_mod_time = 0;
    state->undo_count = 0;
    state->redo_count = 0;
    state->last_auto_save_time = time(NULL);
    state->auto_indent_on_newline = true;
    state->paste_mode = false;
    state->word_wrap_enabled = true;
    state->num_recent_dirs = 0;
    state->recent_dirs = NULL;
    load_directory_history(state);
    char initial_cwd[1024];
    if (getcwd(initial_cwd, sizeof(initial_cwd)) != NULL) {
        update_directory_access(state, initial_cwd);
    }
    load_syntax_file(state, "c.syntax");

    gerenciador.janelas[gerenciador.num_janelas - 1] = nova_janela;
    gerenciador.janela_ativa_idx = gerenciador.num_janelas - 1;

    recalcular_layout_janelas(); // Create the ncurses window FIRST

    // Now that the window exists, we can safely load the file
    if (filename) {
        load_file(state, filename);
    } else {
        state->lines[0] = calloc(1, 1);
        state->num_lines = 1;
    }

    push_undo(state);
}

void redesenhar_todas_as_janelas() {
    erase();
    wnoutrefresh(stdscr);

    for (int i = 0; i < gerenciador.num_janelas; i++) {
        JanelaEditor *jw = gerenciador.janelas[i];
        editor_redraw(jw->win, jw->estado);
        wnoutrefresh(jw->win);
    }

    posicionar_cursor_ativo();
    doupdate();
}

void posicionar_cursor_ativo() {
    if (gerenciador.num_janelas == 0) return;

    JanelaEditor* active_jw = gerenciador.janelas[gerenciador.janela_ativa_idx];
    EditorState* state = active_jw->estado;
    WINDOW* win = active_jw->win;

    if (state->completion_mode != COMPLETION_NONE) {
        editor_draw_completion_win(win, state);
    } else {
        curs_set(1);
        if (state->mode == COMMAND) {
            int rows, cols;
            getmaxyx(win, rows, cols);
            wmove(win, rows - 1, state->command_pos + 2);
        } else {
            int visual_y, visual_x;
            get_visual_pos(win, state, &visual_y, &visual_x);
            int border_offset = gerenciador.num_janelas > 1 ? 1 : 0;
            wmove(win, visual_y - state->top_line + border_offset, visual_x - state->left_col + border_offset);
        }
    }
}

void fechar_janela_ativa(bool *should_exit) {
    if (gerenciador.num_janelas == 0) return;

    int idx = gerenciador.janela_ativa_idx;
    JanelaEditor *jw = gerenciador.janelas[idx];
    EditorState *state = jw->estado;

    if (state->buffer_modified) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
        return;
    }

    free_janela_editor(jw);

    for (int i = idx; i < gerenciador.num_janelas - 1; i++) {
        gerenciador.janelas[i] = gerenciador.janelas[i+1];
    }
    gerenciador.num_janelas--;

    if (gerenciador.num_janelas == 0) {
        *should_exit = true;
        free(gerenciador.janelas);
        gerenciador.janelas = NULL;
        return;
    }

    JanelaEditor **new_janelas = realloc(gerenciador.janelas, sizeof(JanelaEditor*) * gerenciador.num_janelas);
    if (!new_janelas) {
        endwin();
        fprintf(stderr, "Error: Failed to reallocate window manager memory.\n");
        exit(1);
    }
    gerenciador.janelas = new_janelas;
    
    if (gerenciador.janela_ativa_idx >= gerenciador.num_janelas) {
        gerenciador.janela_ativa_idx = gerenciador.num_janelas - 1;
    }

    recalcular_layout_janelas();
}

void proxima_janela() {
    if (gerenciador.num_janelas > 1) {
        gerenciador.janela_ativa_idx = (gerenciador.janela_ativa_idx + 1) % gerenciador.num_janelas;
    }
}

void janela_anterior() {
    if (gerenciador.num_janelas > 1) {
        gerenciador.janela_ativa_idx = (gerenciador.janela_ativa_idx - 1 + gerenciador.num_janelas) % gerenciador.num_janelas;
    }
}


// ===================================================================
// Bracket Matching
// ===================================================================

void editor_find_unmatched_brackets(EditorState *state) {
    if (state->unmatched_brackets) free(state->unmatched_brackets);
    state->unmatched_brackets = NULL;
    state->num_unmatched_brackets = 0;

    BracketStackItem *stack = NULL;
    int stack_top = 0;
    int stack_capacity = 0;

    for (int i = 0; i < state->num_lines; i++) {
        char *line = state->lines[i];
        if (!line) continue;

        bool in_string = false;
        char string_char = 0;

        for (int j = 0; line[j] != '\0'; j++) {
            if (in_string) {
                if (line[j] == '\\') { 
			j++;
			continue;
	       	}
                if (line[j] == string_char) in_string = false;
                continue;
            }
            if (line[j] == '"' || line[j] == '\'') {
                in_string = true;
                string_char = line[j];
                continue;
            }
            if (line[j] == '/' && line[j+1] != '\0' && line[j+1] == '/') break;

            char c = line[j];
            if (c == '(' || c == '[' || c == '{') {
                if (stack_top >= stack_capacity) {
                    stack_capacity = (stack_capacity == 0) ? 8 : stack_capacity * 2;
                    BracketStackItem *new_stack = realloc(stack, stack_capacity * sizeof(BracketStackItem));
                    if (!new_stack) { if (stack) free(stack); return; } // Error handling
                    stack = new_stack;
                }
                stack[stack_top++] = (BracketStackItem){ .line = i, .col = j, .type = c };
            } else if (c == ')' || c == ']' || c == '}') {
                if (stack_top > 0) {
                    char open_bracket = stack[stack_top - 1].type;
                    bool match = (c == ')' && open_bracket == '(') ||
                                 (c == ']' && open_bracket == '[') ||
                                 (c == '}' && open_bracket == '{');
                    if (match) {
                        stack_top--;
                    } else {
                        state->num_unmatched_brackets++;
                        BracketInfo *new_brackets = realloc(state->unmatched_brackets, state->num_unmatched_brackets * sizeof(BracketInfo));
                        if (!new_brackets) return; // Error handling
                        state->unmatched_brackets = new_brackets;
                        state->unmatched_brackets[state->num_unmatched_brackets - 1] = (BracketInfo){ .line = i, .col = j, .type = c };
                    }
                } else {
                    state->num_unmatched_brackets++;
                    BracketInfo *new_brackets = realloc(state->unmatched_brackets, state->num_unmatched_brackets * sizeof(BracketInfo));
                    if (!new_brackets) return; // Error handling
                    state->unmatched_brackets = new_brackets;
                    state->unmatched_brackets[state->num_unmatched_brackets - 1] = (BracketInfo){ .line = i, .col = j, .type = c };
                }
            }
        }
    }

    if (stack_top > 0) {
        int old_num = state->num_unmatched_brackets;
        state->num_unmatched_brackets += stack_top;
        BracketInfo *new_brackets = realloc(state->unmatched_brackets, state->num_unmatched_brackets * sizeof(BracketInfo));
        if (!new_brackets) { if (stack) free(stack); return; } // Error handling
        state->unmatched_brackets = new_brackets;
        for (int k = 0; k < stack_top; k++) {
            state->unmatched_brackets[old_num + k] = (BracketInfo){ .line = stack[k].line, .col = stack[k].col, .type = stack[k].type };
        }
    }

    if (stack) free(stack);
}

bool is_unmatched_bracket(EditorState *state, int line, int col) {
    for (int i = 0; i < state->num_unmatched_brackets; i++) {
        if (state->unmatched_brackets[i].line == line && state->unmatched_brackets[i].col == col) {
            return true;
        }
    }
    return false;
}

// ===================================================================
// 2. File I/O & Handling
// ===================================================================

void load_file_core(EditorState *state, const char *filename) {
    for (int i = 0; i < state->num_lines; i++) { if(state->lines[i]) free(state->lines[i]); state->lines[i] = NULL; }
    state->num_lines = 0;
    strncpy(state->filename, filename, sizeof(state->filename) - 1);
    state->filename[sizeof(state->filename) - 1] = '\0';

    FILE *file = fopen(filename, "r");
    if (file) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), file) && state->num_lines < MAX_LINES) {
            line[strcspn(line, "\n")] = 0;
            state->lines[state->num_lines] = strdup(line);
            if (!state->lines[state->num_lines]) { fclose(file); return; }
            state->num_lines++;
        }
        fclose(file);
        snprintf(state->status_msg, sizeof(state->status_msg), "%s loaded", filename);
    } else {
        if (errno == ENOENT) {
            state->lines[0] = calloc(1, 1);
            if (!state->lines[0]) return; 
            state->num_lines = 1;
            snprintf(state->status_msg, sizeof(state->status_msg), "New file: \"%s\"", filename);
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "Error opening file: %s", strerror(errno));
        }
    }

    if (state->num_lines == 0) {
        state->lines[0] = calloc(1, 1);
        state->num_lines = 1;
    }
    state->current_line = load_last_line(filename);
    if (state->current_line >= state->num_lines) {
        state->current_line = state->num_lines > 0 ? state->num_lines - 1 : 0;
    }
    if (state->current_line < 0) {
        state->current_line = 0;
    }
    state->current_col = 0;
    state->ideal_col = 0;
    state->top_line = 0;
    state->left_col = 0;
    state->buffer_modified = false;
    state->last_file_mod_time = get_file_mod_time(state->filename);
    editor_find_unmatched_brackets(state);
}
   
void load_file(EditorState *state, const char *filename) {
	if (strstr(filename, AUTO_SAVE_EXTENSION) == NULL) {
		char sv_filename[256];
		snprintf(sv_filename, sizeof(sv_filename), "%s%s", filename, AUTO_SAVE_EXTENSION);
		struct stat st;
		if (stat(sv_filename, &st) == 0) {
		    handle_file_recovery(state, filename, sv_filename);
		    return;
		}
	}
	load_file_core(state, filename);
}

void save_file(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) { 
        strncpy(state->status_msg, "No file name. Use :w <filename>", sizeof(state->status_msg) - 1); 
        return; 
    } 
    
    FILE *file = fopen(state->filename, "w");
    if (file) {
        for (int i = 0; i < state->num_lines; i++) {
            if (state->lines[i]) {
                fprintf(file, "%s\n", state->lines[i]);
            }
        }
        fclose(file); 
        
        char auto_save_filename[256];
        snprintf(auto_save_filename, sizeof(auto_save_filename), "%s%s", state->filename, AUTO_SAVE_EXTENSION);
        remove(auto_save_filename);
        
        char display_filename[40]; 
        strncpy(display_filename, state->filename, sizeof(display_filename) - 1); 
        display_filename[sizeof(display_filename) - 1] = '\0';
        snprintf(state->status_msg, sizeof(state->status_msg), "%s written", display_filename);
        state->buffer_modified = false;
        state->last_file_mod_time = get_file_mod_time(state->filename);
    } else { 
        snprintf(state->status_msg, sizeof(state->status_msg), "Error saving: %s", strerror(errno)); 
    } 
}

void auto_save(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) return;
    if (!state->buffer_modified) return;

    char auto_save_filename[256];
    snprintf(auto_save_filename, sizeof(auto_save_filename), "%s%s", state->filename, AUTO_SAVE_EXTENSION);

    FILE *file = fopen(auto_save_filename, "w");
    if (file) {
        for (int i = 0; i < state->num_lines; i++) {
            if (state->lines[i]) {
                fprintf(file, "%s\n", state->lines[i]);
            }
        }
        fclose(file);
    }
}

time_t get_file_mod_time(const char *filename) {
    struct stat attr;
    if (stat(filename, &attr) == 0) {
        return attr.st_mtime;
    }
    return 0;
}

void check_external_modification(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0 || state->last_file_mod_time == 0) {
        return;
    }

    time_t on_disk_mod_time = get_file_mod_time(state->filename);

    if (on_disk_mod_time != 0 && on_disk_mod_time != state->last_file_mod_time) {
        WINDOW *win = gerenciador.janelas[gerenciador.janela_ativa_idx]->win;
        if (state->buffer_modified) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Warning: File on disk has changed! (S)ave, (L)oad, or (C)ancel?");
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "File on disk has changed. Reload? (Y/N)");
        }
        editor_redraw(win, state);
        wint_t ch;
        wget_wch(win, &ch);
        if (state->buffer_modified) {
             switch (tolower(ch)) {
                case 's':
                    save_file(state);
                    break;
                case 'l':
                    load_file(state, state->filename);
                    break;
                default:
                    state->last_file_mod_time = on_disk_mod_time;
                    snprintf(state->status_msg, sizeof(state->status_msg), "Action cancelled. In-memory version kept.");
                    break;
            }
        } else {
            if (tolower(ch) == 'y') {
                load_file(state, state->filename);
            } else {
                state->last_file_mod_time = on_disk_mod_time;
                 snprintf(state->status_msg, sizeof(state->status_msg), "Reload cancelled.");
            }
        }
    }
}

void editor_reload_file(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "No file name to reload.");
        return;
    }
    
    time_t on_disk_mod_time = get_file_mod_time(state->filename);
    
    if (state->buffer_modified && on_disk_mod_time != 0 && on_disk_mod_time != state->last_file_mod_time) {
        // Usar mensagem de status em vez de diálogo interativo
        snprintf(state->status_msg, sizeof(state->status_msg), 
                 "Warning: File changed on disk! Use :rc! to force reload.");
        return;
    }
    
    // Recarregar o arquivo normalmente
    load_file(state, state->filename);
    snprintf(state->status_msg, sizeof(state->status_msg), "File reloaded.");
}

void load_syntax_file(EditorState *state, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) return;

    char line_buffer[256];
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        if (line_buffer[0] == '#' || line_buffer[0] == '\n' || line_buffer[0] == '\r') continue;

        line_buffer[strcspn(line_buffer, "\r\n")] = 0;

        char *colon = strchr(line_buffer, ':');
        if (!colon) continue;

        *colon = '\0'; 
        
        char *type_str = trim_whitespace(line_buffer);
        char *word_str = trim_whitespace(colon + 1);

        if (strlen(type_str) == 0 || strlen(word_str) == 0) continue;

        state->num_syntax_rules++;
        state->syntax_rules = realloc(state->syntax_rules, sizeof(SyntaxRule) * state->num_syntax_rules);
        
        SyntaxRule *new_rule = &state->syntax_rules[state->num_syntax_rules - 1];
        new_rule->word = strdup(word_str);

        if (strcmp(type_str, "KEYWORD") == 0) {
            new_rule->type = SYNTAX_KEYWORD;
        } else if (strcmp(type_str, "TYPE") == 0) {
            new_rule->type = SYNTAX_TYPE;
        } else if (strcmp(type_str, "STD_FUNCTION") == 0) {
            new_rule->type = SYNTAX_STD_FUNCTION;
        } else {
            free(new_rule->word);
            state->num_syntax_rules--;
        }
    }
    fclose(file);
}

void save_last_line(const char *filename, int line) {
    char pos_filename[256];
    snprintf(pos_filename, sizeof(pos_filename), "%s.pos", filename);
    FILE *f = fopen(pos_filename, "w");
    if (f) {
        fprintf(f, "%d", line);
        fclose(f);
    }
}

int load_last_line(const char *filename) {
    char pos_filename[256];
    snprintf(pos_filename, sizeof(pos_filename), "%s.pos", filename);
    FILE *f = fopen(pos_filename, "r");
    if (f) {
        int line = 0;
        fscanf(f, "%d", &line);
        fclose(f);
        return line;
    }
    return 0;
}

// ===================================================================
// 3. File Recovery
// ===================================================================

FileRecoveryChoice display_recovery_prompt(WINDOW *parent_win, EditorState *state) {
    int rows, cols;
    getmaxyx(parent_win, rows, cols);
    
    // Criar uma janela de diálogo
    int win_height = 7;
    int win_width = 50;
    int start_y = (rows - win_height) / 2;
    int start_x = (cols - win_width) / 2;
    
    WINDOW *dialog_win = newwin(win_height, win_width, start_y, start_x);
    keypad(dialog_win, TRUE);
    wbkgd(dialog_win, COLOR_PAIR(9));
    box(dialog_win, 0, 0);
    
    // Exibir mensagem
    mvwprintw(dialog_win, 1, 2, "Arquivo de recuperação encontrado!");
    mvwprintw(dialog_win, 2, 2, "Escolha uma opção:");
    mvwprintw(dialog_win, 3, 4, "(R)ecover from .sv");
    mvwprintw(dialog_win, 4, 4, "(O)pen original");
    mvwprintw(dialog_win, 5, 4, "(D)iff files");
    mvwprintw(dialog_win, 6, 4, "(I)gnore | (Q)uit");
    
    wrefresh(dialog_win);
    
    FileRecoveryChoice choice = RECOVER_ABORT;
    bool decided = false;
    
    while (!decided) {
        wint_t ch;
        wget_wch(dialog_win, &ch);
        ch = tolower(ch);
        
        switch (ch) {
            case 'r': choice = RECOVER_FROM_SV; decided = true; break;
            case 'o': choice = RECOVER_OPEN_ORIGINAL; decided = true; break;
            case 'd': choice = RECOVER_DIFF; decided = true; break;
            case 'i': choice = RECOVER_IGNORE; decided = true; break;
            case 'q': case 27: choice = RECOVER_ABORT; decided = true; break;
        }
    }
    
    delwin(dialog_win);
    return choice;
}

void handle_file_recovery(EditorState *state, const char *original_filename, const char *sv_filename) {
    WINDOW *win = gerenciador.janelas[gerenciador.janela_ativa_idx]->win;
    
    while (1) {
        FileRecoveryChoice choice = display_recovery_prompt(win, state);
        
        switch (choice) {
            case RECOVER_DIFF: {
                char diff_command[1024];
                snprintf(diff_command, sizeof(diff_command), "git diff --no-index -- %s %s", original_filename, sv_filename);
                run_and_display_command(diff_command, "--- DIFERENÇAS ---");
                break; 
            }
            case RECOVER_FROM_SV:
                load_file_core(state, sv_filename);
                strncpy(state->filename, original_filename, sizeof(state->filename) - 1);
                state->buffer_modified = true;
                remove(sv_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Recuperado de %s. Salve para confirmar.", sv_filename);
                return;

            case RECOVER_OPEN_ORIGINAL:
                remove(sv_filename);
                load_file_core(state, original_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Arquivo de recuperação ignorado e removido.");
                return;

            case RECOVER_IGNORE:
                load_file_core(state, original_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Arquivo de recuperação mantido.");
                return;

            case RECOVER_ABORT:
                state->status_msg[0] = '\0';
                state->num_lines = 1;
                state->lines[0] = calloc(1, 1);
                strcpy(state->filename, "[No Name]");
                return;
        }
    }
}

// ===================================================================
// 4. Directory Navigation
// ===================================================================

void get_history_filename(char *buffer, size_t size) {
    const char *home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(buffer, size, "%s/.jntd_dir_history", home_dir);
    } else {
        snprintf(buffer, size, ".jntd_dir_history");
    }
}

int compare_dirs(const void *a, const void *b) {
    DirectoryInfo *dir_a = *(DirectoryInfo**)a;
    DirectoryInfo *dir_b = *(DirectoryInfo**)b;
    return dir_b->access_count - dir_a->access_count;
}

void load_directory_history(EditorState *state) {
    state->recent_dirs = NULL;
    state->num_recent_dirs = 0;

    char history_file[1024];
    get_history_filename(history_file, sizeof(history_file));

    FILE *f = fopen(history_file, "r");
    if (!f) return;

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        int count;
        char path[1024];
        if (sscanf(line, "%d %1023[^\n]", &count, path) == 2) {
            DirectoryInfo *new_dir = malloc(sizeof(DirectoryInfo));
            if (!new_dir) continue;
            
            new_dir->path = strdup(path);
            if (!new_dir->path) { free(new_dir); continue; }
            
            new_dir->access_count = count;

            state->num_recent_dirs++;
            state->recent_dirs = realloc(state->recent_dirs, sizeof(DirectoryInfo*) * state->num_recent_dirs);
            if (!state->recent_dirs) {
                free(new_dir->path);
                free(new_dir);
                state->num_recent_dirs--;
                break;
            }
            
            state->recent_dirs[state->num_recent_dirs - 1] = new_dir;
        }
    }
    fclose(f);

    if (state->num_recent_dirs > 0) {
        qsort(state->recent_dirs, state->num_recent_dirs, sizeof(DirectoryInfo*), compare_dirs);
    }
}

void save_directory_history(EditorState *state) {
    char history_file[1024];
    get_history_filename(history_file, sizeof(history_file));

    FILE *f = fopen(history_file, "w");
    if (!f) return;

    for (int i = 0; i < state->num_recent_dirs; i++) {
        fprintf(f, "%d %s\n", state->recent_dirs[i]->access_count, state->recent_dirs[i]->path);
    }
    fclose(f);
}

void update_directory_access(EditorState *state, const char *path) {
    char canonical_path[PATH_MAX];
    if (realpath(path, canonical_path) == NULL) {
        strncpy(canonical_path, path, sizeof(canonical_path)-1);
        canonical_path[sizeof(canonical_path)-1] = '\0';
    }

    for (int i = 0; i < state->num_recent_dirs; i++) {
        if (strcmp(state->recent_dirs[i]->path, canonical_path) == 0) {
            state->recent_dirs[i]->access_count++;
            qsort(state->recent_dirs, state->num_recent_dirs, sizeof(DirectoryInfo*), compare_dirs);
            save_directory_history(state);
            return;
        }
    }

    state->num_recent_dirs++;
    state->recent_dirs = realloc(state->recent_dirs, sizeof(DirectoryInfo*) * state->num_recent_dirs);

    DirectoryInfo *new_dir = malloc(sizeof(DirectoryInfo));
    new_dir->path = strdup(canonical_path);
    new_dir->access_count = 1;

    state->recent_dirs[state->num_recent_dirs - 1] = new_dir;

    qsort(state->recent_dirs, state->num_recent_dirs, sizeof(DirectoryInfo*), compare_dirs);
    save_directory_history(state);
}

void change_directory(EditorState *state, const char *new_path) {
    if (chdir(new_path) == 0) {
        update_directory_access(state, new_path);
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Diretório mudado para: %s", cwd);
        }
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Erro ao mudar para: %s", strerror(errno));
    }
}

void display_directory_navigator(EditorState *state) {
    if (state->num_recent_dirs == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "No recent directories available.");
        return;
    }

    WINDOW *nav_win;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int win_h = min(state->num_recent_dirs + 4, rows - 4);
    if (win_h < 10) win_h = 10;
    int win_w = cols - 10;
    if (win_w < 50) win_w = 50;
    int win_y = (rows - win_h) / 2;
    int win_x = (cols - win_w) / 2;

    nav_win = newwin(win_h, win_w, win_y, win_x);
    if (!nav_win) return;
    
    keypad(nav_win, TRUE);
    wbkgd(nav_win, COLOR_PAIR(9));
    box(nav_win, 0, 0);

    mvwprintw(nav_win, 1, (win_w - 25) / 2, "Navegador de Diretórios");
    
    int current_selection = 0;
    int top_of_list = 0;
    int max_visible = win_h - 4;

    while (1) {
        werase(nav_win);
        box(nav_win, 0, 0);
        mvwprintw(nav_win, 1, (win_w - 25) / 2, "Navegador de Diretórios");

        for (int i = 0; i < max_visible; i++) {
            int dir_idx = top_of_list + i;
            if (dir_idx < state->num_recent_dirs) {
                if (dir_idx == current_selection) wattron(nav_win, A_REVERSE);
                
                char display_path[win_w - 4];
                strncpy(display_path, state->recent_dirs[dir_idx]->path, sizeof(display_path) - 1);
                display_path[sizeof(display_path) - 1] = '\0';
                
                if (strlen(state->recent_dirs[dir_idx]->path) > sizeof(display_path) - 1) {
                    strcpy(display_path + sizeof(display_path) - 4, "...");
                }
                
                mvwprintw(nav_win, i + 2, 2, "%s (%d acessos)", 
                         display_path, state->recent_dirs[dir_idx]->access_count);
                
                if (dir_idx == current_selection) wattroff(nav_win, A_REVERSE);
            }
        }

        mvwprintw(nav_win, win_h - 2, 2, "Use as setas para navegar, ENTER para selecionar, ESC para sair.");
        wrefresh(nav_win);

        int ch = wgetch(nav_win);
        switch(ch) {
            case KEY_UP:
                if (current_selection > 0) {
                    current_selection--;
                    if(current_selection < top_of_list) top_of_list = current_selection;
                }
                break;
            case KEY_DOWN:
                if (current_selection < state->num_recent_dirs - 1) {
                    current_selection++;
                    if(current_selection >= top_of_list + max_visible) top_of_list = current_selection - max_visible + 1;
                }
                break;
            case KEY_ENTER: case '\n': case '\r':
                if (current_selection < state->num_recent_dirs) {
                    change_directory(state, state->recent_dirs[current_selection]->path);
                    goto end_nav;
                }
                break;
            case 27: case 'q': case 'Q': goto end_nav;
            case KEY_NPAGE:
                current_selection = min(current_selection + max_visible, state->num_recent_dirs - 1);
                top_of_list = min(top_of_list + max_visible, state->num_recent_dirs - max_visible);
                break;
            case KEY_PPAGE:
                current_selection = max(current_selection - max_visible, 0);
                top_of_list = max(top_of_list - max_visible, 0);
                break;
        }
    }

end_nav:
    delwin(nav_win);
    touchwin(stdscr);
    redesenhar_todas_as_janelas();
}

void prompt_for_directory_change(EditorState *state) {
    if (state->buffer_modified) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Unsaved changes. Proceed with directory change? (y/n)");
        redesenhar_todas_as_janelas();
        wint_t ch;
        wget_wch(gerenciador.janelas[gerenciador.janela_ativa_idx]->win, &ch);
        if (tolower(ch) != 'y') {
            snprintf(state->status_msg, sizeof(state->status_msg), "Cancelled.");
            redesenhar_todas_as_janelas();
            return;
        }
    }

    int rows, cols; getmaxyx(stdscr, rows, cols);
    int win_h = 5; int win_w = cols - 20; if (win_w < 50) win_w = 50;
    int win_y = (rows - win_h) / 2; int win_x = (cols - win_w) / 2;
    WINDOW *input_win = newwin(win_h, win_w, win_y, win_x);
    keypad(input_win, TRUE);
    wbkgd(input_win, COLOR_PAIR(9));
    box(input_win, 0, 0);

    mvwprintw(input_win, 1, 2, "Change to directory:");
    wrefresh(input_win);

    char path_buffer[1024] = {0};
    curs_set(1); echo(); 
    wmove(input_win, 2, 2);
    wgetnstr(input_win, path_buffer, sizeof(path_buffer) - 1);
    noecho(); curs_set(0);

    delwin(input_win);
    touchwin(stdscr);

    if (strlen(path_buffer) > 0) {
        change_directory(state, path_buffer);
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "No path entered. Cancelled.");
    }
    
    redesenhar_todas_as_janelas();
}

// ===================================================================
// 5. Command Execution & Processing
// ===================================================================

void process_command(EditorState *state, bool *should_exit) {
    if (state->command_buffer[0] == '!') {
        execute_shell_command(state);
        add_to_command_history(state, state->command_buffer);
        state->mode = NORMAL;
        return;
    }
    add_to_command_history(state, state->command_buffer);
    char command[100], args[1024] = "";
    char *buffer_ptr = state->command_buffer;
    int i = 0;
    while(i < 99 && *buffer_ptr && !isspace(*buffer_ptr)) command[i++] = *buffer_ptr++;
    command[i] = '\0';
    if(isspace(*buffer_ptr)) buffer_ptr++;
    char *trimmed_args = trim_whitespace(buffer_ptr);
    strncpy(args, trimmed_args, sizeof(args) - 1);
    args[sizeof(args)-1] = '\0';

    if (strcmp(command, "q") == 0) {
        if (state->buffer_modified) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
            state->mode = NORMAL;
            return;
        }
        fechar_janela_ativa(should_exit);
        return; 
    } else if (strcmp(command, "q!") == 0) {
        state->buffer_modified = false;
        fechar_janela_ativa(should_exit);
        return;
    } else if (strcmp(command, "wq") == 0) {
        save_file(state);
        fechar_janela_ativa(should_exit);
        return;
    } else if (strcmp(command, "w") == 0) {
        if (strlen(args) > 0) strncpy(state->filename, args, sizeof(state->filename) - 1);
        save_file(state);
    } else if (strcmp(command, "help") == 0) {
        display_help_screen();
    } else if (strcmp(command, "gcc") == 0) {
        compile_file(state, args);
    } else if (strcmp(command, "rc") == 0) {
        editor_reload_file(state);
    } else if (strcmp(command, "rc!") == 0) {
        if (strcmp(state->filename, "[No Name]") == 0) {
            snprintf(state->status_msg, sizeof(state->status_msg), "No file name to reload.");
    } else {
            load_file(state, state->filename);
            snprintf(state->status_msg, sizeof(state->status_msg), "File reloaded (force).");
        }
    } else if (strcmp(command, "open") == 0) {
        if (strlen(args) > 0) load_file(state, args);
        else snprintf(state->status_msg, sizeof(state->status_msg), "Usage: :open <filename>");
    } else if (strcmp(command, "new") == 0) {
        for (int i = 0; i < state->num_lines; i++) { if(state->lines[i]) 
        free(state->lines[i]); state->lines[i] = NULL; }
        state->num_lines = 1; state->lines[0] = calloc(1, 1); strcpy(state->filename, "[No Name]");
        state->current_line = 0; state->current_col = 0; state->ideal_col = 0; state->top_line = 0; state->left_col = 0;
        snprintf(state->status_msg, sizeof(state->status_msg), "New file opened.");
    } else if (strcmp(command, "timer") == 0) {
        def_prog_mode(); endwin();
        display_work_summary();
        reset_prog_mode(); refresh();
    } else if (strcmp(command, "diff") == 0) {
        diff_command(state, args);
    } else if (strcmp(command, "set") == 0) {
        if (strcmp(args, "paste") == 0) {
            state->paste_mode = true;
            state->auto_indent_on_newline = false;
            snprintf(state->status_msg, sizeof(state->status_msg), "-- PASTE MODE ON --");
        } else if (strcmp(args, "nopaste") == 0) {
            state->paste_mode = false;
            state->auto_indent_on_newline = true;
            snprintf(state->status_msg, sizeof(state->status_msg), "-- PASTE MODE OFF --");
        } else if (strcmp(args, "wrap") == 0) {
            state->word_wrap_enabled = true;
            snprintf(state->status_msg, sizeof(state->status_msg), "Word wrap ativado");
        } else if (strcmp(args, "nowrap") == 0) {
            state->word_wrap_enabled = false;
            snprintf(state->status_msg, sizeof(state->status_msg), "Word wrap desativado");
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "Argumento desconhecido para set: %s", args);
        }
    } else if (strcmp(command, "toggle_auto_indent") == 0) {
        state->auto_indent_on_newline = !state->auto_indent_on_newline;
        snprintf(state->status_msg, sizeof(state->status_msg), "Auto-indent on newline: %s", state->auto_indent_on_newline ? "ON" : "OFF");
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Unknown command: %s", command);
    }
    state->mode = NORMAL;
}

void execute_shell_command(EditorState *state) {
    char *cmd = state->command_buffer + 1;
    if (strncmp(cmd, "cd ", 3) == 0) {
        char *path = cmd + 3;
        if (chdir(path) != 0) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Erro ao mudar diretório: %s", strerror(errno));
        } else {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
                char display_cwd[80];
                strncpy(display_cwd, cwd, sizeof(display_cwd) - 1);
                display_cwd[sizeof(display_cwd) - 1] = '\0';
                snprintf(state->status_msg, sizeof(state->status_msg), "Diretório atual: %s", display_cwd);
            }
        }
        return;
    }
    char temp_output_file[] = "/tmp/editor_shell_output.XXXXXX";
    int fd = mkstemp(temp_output_file);
    if(fd == -1) { snprintf(state->status_msg, sizeof(state->status_msg), "Erro ao criar arquivo temporário."); return; }
    close(fd);
    char full_shell_command[2048];
    snprintf(full_shell_command, sizeof(full_shell_command), "%s > %s 2>&1", cmd, temp_output_file);
    def_prog_mode(); endwin();
    system(full_shell_command);
    reset_prog_mode(); refresh();
    FILE *f = fopen(temp_output_file, "r");
    if(f) {
        fseek(f, 0, SEEK_END); long size = ftell(f); fclose(f);
        if (size > 0 && size < 96) {
            FILE *read_f = fopen(temp_output_file, "r");
            char buffer[STATUS_MSG_LEN] = {0};
            size_t n = fread(buffer, 1, sizeof(buffer) - 1, read_f);
            fclose(read_f);
            if (n > 0 && buffer[n-1] == '\n') buffer[n-1] = '\0';
            if(strchr(buffer, '\n') == NULL) {
                char display_output[80];
                strncpy(display_output, buffer, sizeof(display_output) - 1);
                display_output[sizeof(display_output) - 1] = '\0';
                snprintf(state->status_msg, sizeof(state->status_msg), "Saída: %s", display_output);
                remove(temp_output_file);
                return;
            }
        }
        display_output_screen("--- SAÍDA DO COMANDO ---", temp_output_file);
        snprintf(state->status_msg, sizeof(state->status_msg), "Comando '%s' executado.", cmd);
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Comando executado, mas sem saída.");
        remove(temp_output_file);
    }
}

void compile_file(EditorState *state, char* args) {
    save_file(state);
    if (strcmp(state->filename, "[No Name]") == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Salve o arquivo com um nome antes de compilar.");
        return;
    }
    char output_filename[300];
    strncpy(output_filename, state->filename, sizeof(output_filename) - 1);
    char *dot = strrchr(output_filename, '.'); if (dot) *dot = '\0';
    char command[1024];
    snprintf(command, sizeof(command), "gcc %s -o %s %s", state->filename, output_filename, args);
    char temp_output_file[] = "/tmp/editor_compile_output.XXXXXX";
    int fd = mkstemp(temp_output_file); if(fd == -1) return; close(fd);
    char full_shell_command[2048];
    snprintf(full_shell_command, sizeof(full_shell_command), "%s > %s 2>&1", command, temp_output_file);
    def_prog_mode(); endwin();
    int ret = system(full_shell_command);
    reset_prog_mode(); refresh();
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        char display_output_name[40];
        strncpy(display_output_name, output_filename, sizeof(display_output_name) - 1);
        display_output_name[sizeof(display_output_name)-1] = '\0';
        snprintf(state->status_msg, sizeof(state->status_msg), "Compilação bem-sucedida! Executável: %s", display_output_name);
    } else {
        display_output_screen("--- ERROS DE COMPILAÇÃO ---", temp_output_file);
        snprintf(state->status_msg, sizeof(state->status_msg), "Compilação falhou. Veja os erros.");
    }
}

void run_and_display_command(const char* command, const char* title) {
    char temp_output_file[] = "/tmp/editor_cmd_output.XXXXXX";
    int fd = mkstemp(temp_output_file);
    if (fd == -1) return;
    close(fd);

    char full_shell_command[2048];
    snprintf(full_shell_command, sizeof(full_shell_command), "%s > %s 2>&1", command, temp_output_file);

    def_prog_mode(); endwin();
    system(full_shell_command);
    reset_prog_mode(); refresh();
    
    display_output_screen(title, temp_output_file);
}

void diff_command(EditorState *state, const char *args) {
    char filename1[256] = {0}, filename2[256] = {0};
    if (sscanf(args, "%255s %255s", filename1, filename2) != 2) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Uso: :diff <arquivo1> <arquivo2>");
        return;
    }
    char diff_cmd_str[1024];
    snprintf(diff_cmd_str, sizeof(diff_cmd_str), "git diff --no-index -- %s %s", filename1, filename2);
    run_and_display_command(diff_cmd_str, "--- Diferenças ---");
}

void add_to_command_history(EditorState *state, const char* command) {
    if (strlen(command) == 0) return;
    if (state->history_count > 0 && strcmp(state->command_history[state->history_count - 1], command) == 0) return;
    if (state->history_count < MAX_COMMAND_HISTORY) {
        state->command_history[state->history_count++] = strdup(command);
    } else {
        free(state->command_history[0]);
        for (int i = 0; i < MAX_COMMAND_HISTORY - 1; i++) {
            state->command_history[i] = state->command_history[i + 1];
        }
        state->command_history[MAX_COMMAND_HISTORY - 1] = strdup(command);
    }
}

// ===================================================================
// 6. Screen & UI
// ===================================================================

void editor_redraw(WINDOW *win, EditorState *state) {
    if (state->buffer_modified) {
        editor_find_unmatched_brackets(state);
    }

    werase(win);
    int rows, cols;
    getmaxyx(win, rows, cols);

    int border_offset = gerenciador.num_janelas > 1 ? 1 : 0;

    if (border_offset) {
        if (gerenciador.janelas[gerenciador.janela_ativa_idx]->estado == state) {
            wattron(win, COLOR_PAIR(3) | A_BOLD);
            box(win, 0, 0);
            wattroff(win, COLOR_PAIR(3) | A_BOLD);
        } else {
            box(win, 0, 0);
        }
    }
    
    adjust_viewport(win, state);

    const char *delimiters = " \t\n\r,;()[]{}<>=+-*/%&|!^.";
    int content_height = rows - (border_offset + 1); 
    int screen_y = 0;

    if (state->word_wrap_enabled) {
        state->left_col = 0;
        int visual_line_idx = 0;
        for (int file_line_idx = 0; file_line_idx < state->num_lines && screen_y < content_height; file_line_idx++) {
            char *line = state->lines[file_line_idx];
            if (!line) continue;

            int line_len = strlen(line);
            if (line_len == 0) {
                if (visual_line_idx >= state->top_line) {
                    wmove(win, screen_y + border_offset, border_offset);
                    int y, x;
                    getyx(win, y, x);
                    int end_col = cols - border_offset;
                    for (int i = x; i < end_col; i++) {
                        mvwaddch(win, y, i, ' ');
                    }
                    screen_y++;
                }
                visual_line_idx++;
                continue;
            }

            int line_offset = 0;
            while(line_offset < line_len || line_len == 0) {
                int content_width = cols - 2*border_offset;
                int current_bytes = 0;
                int current_width = 0;
                int last_space_bytes = -1;

                while (line[line_offset + current_bytes] != '\0') {
                    wchar_t wc;
                    int bytes_consumed = mbtowc(&wc, &line[line_offset + current_bytes], MB_CUR_MAX);
                    if (bytes_consumed <= 0) { bytes_consumed = 1; wc = ' '; }
                    
                    int char_width = wcwidth(wc);
                    if (char_width < 0) char_width = 1;
                    if (current_width + char_width > content_width) break;

                    current_width += char_width;
                    if (iswspace(wc)) {
                        last_space_bytes = current_bytes + bytes_consumed;
                    }
                    current_bytes += bytes_consumed;
                }

                int break_pos;
                if (line[line_offset + current_bytes] != '\0' && last_space_bytes != -1) {
                    break_pos = last_space_bytes;
                } else {
                    break_pos = current_bytes;
                }

                if (break_pos == 0 && line_offset + current_bytes < line_len) {
                    break_pos = current_bytes;
                }

                if (visual_line_idx >= state->top_line && screen_y < content_height) {
                    wmove(win, screen_y + border_offset, border_offset);
                    int current_pos_in_segment = 0;
                    while(current_pos_in_segment < break_pos) {
                        if (getcurx(win) >= cols - 1 - border_offset) break;
                        int token_start_in_line = line_offset + current_pos_in_segment;
                        if (line[token_start_in_line] == '#' || (line[token_start_in_line] == '/' && (size_t)token_start_in_line + 1 < strlen(line) && line[token_start_in_line + 1] == '/')) {
                            wattron(win, COLOR_PAIR(6)); mvwprintw(win, screen_y + border_offset, getcurx(win), "%.*s", cols - getcurx(win) - border_offset, &line[token_start_in_line]); wattroff(win, COLOR_PAIR(6)); break;
                        }
                        int token_start_in_segment = current_pos_in_segment;
                        if (strchr(delimiters, line[token_start_in_line])) {
                            current_pos_in_segment++;
                        } else {
                            while(current_pos_in_segment < break_pos && !strchr(delimiters, line[line_offset + current_pos_in_segment])) current_pos_in_segment++;
                        }
                        int token_len = current_pos_in_segment - token_start_in_segment;
                        if (token_len > 0) {
                            char *token_ptr = &line[token_start_in_line];
                            int color_pair = 0;

                            if (token_len == 1 && is_unmatched_bracket(state, file_line_idx, token_start_in_line)) {
                                color_pair = 11; // Red
                            } else if (!strchr(delimiters, *token_ptr)) {
                                for (int j = 0; j < state->num_syntax_rules; j++) {
                                    if (strlen(state->syntax_rules[j].word) == (size_t)token_len && strncmp(token_ptr, state->syntax_rules[j].word, token_len) == 0) {
                                        switch(state->syntax_rules[j].type) {
                                            case SYNTAX_KEYWORD: color_pair = 3; break;
                                            case SYNTAX_TYPE: color_pair = 4; break;
                                            case SYNTAX_STD_FUNCTION: color_pair = 5; break;
                                        }
                                        break;
                                    }
                                }
                            }
                            if (color_pair) wattron(win, COLOR_PAIR(color_pair));
                            int remaining_width = (cols - 1 - border_offset) - getcurx(win);
                            if (token_len > remaining_width) token_len = remaining_width;
                            if (token_len > 0) wprintw(win, "%.*s", token_len, token_ptr);
                            if (color_pair) wattroff(win, COLOR_PAIR(color_pair));
                        }
                    }
                    int y, x;
                    getyx(win, y, x);
                    int end_col = cols - border_offset;
                    for (int i = x; i < end_col; i++) {
                        mvwaddch(win, y, i, ' ');
                    }
                    screen_y++;
                }
                visual_line_idx++;
                line_offset += break_pos;
                if (line_len == 0) break;
            }
        }
    } else {
        for (int line_idx = state->top_line; line_idx < state->num_lines && screen_y < content_height; line_idx++) {
            char *line = state->lines[line_idx];
            if (!line) continue;
            
            wmove(win, screen_y + border_offset, border_offset);
            int line_len = strlen(line);
            int current_col = 0;

            while(current_col < line_len) {
                if (current_col < state->left_col) { current_col++; continue; }
                if (getcurx(win) >= cols - 1 - border_offset) break;

                int token_start = current_col;
                char current_char = line[token_start];
                int token_len;

                if (strchr(delimiters, current_char)) {
                    token_len = 1;
                } else {
                    int end = token_start;
                    while(end < line_len && !strchr(delimiters, line[end])) end++;
                    token_len = end - token_start;
                }

                char *token_ptr = &line[token_start];
                int color_pair = 0;

                if (token_len == 1 && is_unmatched_bracket(state, line_idx, token_start)) {
                    color_pair = 11; // Red
                } else if (current_char == '#' || (current_char == '/' && (size_t)token_start + 1 < strlen(line) && line[token_start + 1] == '/')) {
                    color_pair = 6;
                    token_len = line_len - token_start;
                } else if (!strchr(delimiters, current_char)) {
                    for (int j = 0; j < state->num_syntax_rules; j++) {
                        if (strlen(state->syntax_rules[j].word) == (size_t)token_len && strncmp(token_ptr, state->syntax_rules[j].word, token_len) == 0) {
                            switch(state->syntax_rules[j].type) {
                                case SYNTAX_KEYWORD: color_pair = 3; break;
                                case SYNTAX_TYPE: color_pair = 4; break;
                                case SYNTAX_STD_FUNCTION: color_pair = 5; break;
                            }
                            break;
                        }
                    }
                }

                if (color_pair) wattron(win, COLOR_PAIR(color_pair));

                int remaining_width = (cols - 1 - border_offset) - getcurx(win);
                if (token_len > remaining_width) token_len = remaining_width;
                if (token_len > 0) wprintw(win, "%.*s", token_len, token_ptr);

                if (color_pair) wattroff(win, COLOR_PAIR(color_pair));

                current_col += token_len;
            }
            int y, x;
            getyx(win, y, x);
            int end_col = cols - border_offset;
            for (int i = x; i < end_col; i++) {
                mvwaddch(win, y, i, ' ');
            }
            screen_y++;
        }
    }

    int color_pair = 1; // Padrão: azul
    if (strstr(state->status_msg, "Warning:") != NULL || 
        strstr(state->status_msg, "Error:") != NULL) {
        color_pair = 3; // Amarelo para avisos
    }
    
    wattron(win, COLOR_PAIR(color_pair));
    
    for (int i = 1; i < cols - 1; i++) {
        mvwaddch(win, rows - 1, i, ' ');
    }
    
    if (state->mode == COMMAND) {
        mvwprintw(win, rows - 1, 1, ":%.*s", cols-2, state->command_buffer);
    } else {
        char mode_str[20];
        switch (state->mode) {
            case NORMAL: strcpy(mode_str, "-- NORMAL --"); break; 
            case INSERT: strcpy(mode_str, "-- INSERT --"); break;
            default: strcpy(mode_str, "--          --"); break;
        }
        char display_filename[40];
        strncpy(display_filename, state->filename, sizeof(display_filename) - 1);
        display_filename[sizeof(display_filename) - 1] = '\0'; 
        int visual_col = get_visual_col(state->lines[state->current_line], state->current_col);
        
        char final_bar[cols + 1];
        snprintf(final_bar, sizeof(final_bar), "%s | %s%s | %s | Line %d/%d, Col %d", 
            mode_str, display_filename, state->buffer_modified ? "*" : "", 
            state->status_msg, state->current_line + 1, state->num_lines, visual_col + 1);

        mvwprintw(win, rows - 1, 1, "%.*s", cols - 2, final_bar);
    }
    wattroff(win, COLOR_PAIR(color_pair)); 
}

void adjust_viewport(WINDOW *win, EditorState *state) {
    int rows, cols; getmaxyx(win, rows, cols); 
    
    int visual_y, visual_x;
    get_visual_pos(win, state, &visual_y, &visual_x);

    int content_height = rows - (gerenciador.num_janelas > 1 ? 2 : 1);

    if (state->word_wrap_enabled) {
        if (visual_y < state->top_line) {
            state->top_line = visual_y;
        }
        if (visual_y >= state->top_line + content_height) {
            state->top_line = visual_y - content_height + 1;
        }
    } else {
        if (state->current_line < state->top_line) {
            state->top_line = state->current_line;
        }
        if (state->current_line >= state->top_line + content_height) {
            state->top_line = state->current_line - content_height + 1;
        }
        if (visual_x < state->left_col) {
            state->left_col = visual_x;
        }
        if (visual_x >= state->left_col + cols) {
            state->left_col = visual_x - cols + 1;
        }
    }
}

void get_visual_pos(WINDOW *win, EditorState *state, int *visual_y, int *visual_x) {
    int rows, cols; getmaxyx(win, rows, cols);
    int content_width = cols - (gerenciador.num_janelas > 1 ? 2 : 0);

    int y = 0;
    int x = 0;

    if (state->word_wrap_enabled) {
        for (int i = 0; i < state->current_line; i++) {
            char* line = state->lines[i];
            int line_len = strlen(line);
            if (line_len == 0) {
                y++;
            } else {
                int line_offset = 0;
                while(line_offset < line_len) {
                    int break_pos = (line_len - line_offset > content_width) ? content_width : line_len - line_offset;
                     if (line_offset + break_pos < line_len) {
                        int temp_break = -1;
                        for (int j = break_pos - 1; j >= 0; j--) {
                            if (isspace(line[line_offset + j])) { temp_break = j; break; }
                        }
                        if (temp_break != -1) break_pos = temp_break + 1;
                    }
                    line_offset += break_pos;
                    y++;
                }
            }
        }
        
        int current_line_offset = 0;
        while(current_line_offset + content_width < state->current_col) {
             int break_pos = content_width;
             if (current_line_offset + content_width < strlen(state->lines[state->current_line])) {
                int temp_break = -1;
                for (int j = content_width - 1; j >= 0; j--) {
                    if (isspace(state->lines[state->current_line][current_line_offset + j])) { temp_break = j; break; }
                }
                if (temp_break != -1) break_pos = temp_break + 1;
            }
            current_line_offset += break_pos;
            y++;
        }
        x = get_visual_col(state->lines[state->current_line] + current_line_offset, state->current_col - current_line_offset);
        
    } else {
        y = state->current_line;
        x = get_visual_col(state->lines[state->current_line], state->current_col);
    }
    *visual_y = y;
    *visual_x = x;
}

int get_visual_col(const char *line, int byte_col) {
    if (!line) return 0;
    int visual_col = 0;
    int i = 0;
    while (i < byte_col) {
        if (line[i] == '\t') {
            visual_col += TAB_SIZE - (visual_col % TAB_SIZE);
            i++;
        } else {
            wchar_t wc;
            int bytes_consumed = mbtowc(&wc, &line[i], MB_CUR_MAX);
            
            if (bytes_consumed <= 0) {
                visual_col++;
                i++;
            } else {
                int char_width = wcwidth(wc);
                
                visual_col += (char_width > 0) ? char_width : 1;
                i += bytes_consumed;
            }
       }
   }
   return visual_col;
}

void display_help_screen() {
    static const CommandInfo commands[] = {
        { ":w", "Save the current file." },
        { ":w <name>", "Save with a new name." },
        { ":q", "Exit." },
        { ":wq", "Save and exit" },
        { ":open <name>", "Open a file" },
        { ":new", "Creates a blank file." },
        { ":help", "Show this help screen" },
        { ":gcc [libs]", "Compile the current file, (ex: :gcc -lm)." },
        { "![cmd]", "Execute a command in the shell, (ex: !ls -l)." },
        { ":rc", "Reload the current file." } ,
        { ":diff", "Show the difference between 2 files, <ex: (diff a2.c a1.c)," },
        { ":set paste", "Enable paste mode to prevent auto-indent on paste." } ,
        { ":set nopaste", "Disable paste mode and re-enable auto-indent." } ,
        { ":timer", "Show the timer, it count the passed time using the editor." }
    };
    
    int num_commands = sizeof(commands) / sizeof(commands[0]);
    
    WINDOW *help_win = newwin(0, 0, 0, 0);
    wbkgd(help_win, COLOR_PAIR(8));

    wattron(help_win, A_BOLD); mvwprintw(help_win, 2, 2, "--- AJUDA DO EDITOR ---"); wattroff(help_win, A_BOLD);
    
    for (int i = 0; i < num_commands; i++) {
        wmove(help_win, 4 + i, 4);
        wattron(help_win, COLOR_PAIR(3) | A_BOLD);
        wprintw(help_win, "% -15s", commands[i].command);
        wattroff(help_win, COLOR_PAIR(3) | A_BOLD);
        wprintw(help_win, ": %s", commands[i].description);
    }
    
    wattron(help_win, A_REVERSE); mvwprintw(help_win, 6 + num_commands, 2, " Pressione qualquer tecla para voltar ao editor "); wattroff(help_win, A_REVERSE);
    wrefresh(help_win); wgetch(help_win);
    delwin(help_win);
}

void display_output_screen(const char *title, const char *filename) {
    FileViewer *viewer = create_file_viewer(filename);
    if (!viewer) { if(filename) remove(filename); return; }
    
    WINDOW *output_win = newwin(0, 0, 0, 0);
    keypad(output_win, TRUE);
    wbkgd(output_win, COLOR_PAIR(8));

    int top_line = 0;
    wint_t ch;
    while (1) {
        int rows, cols;
        getmaxyx(output_win, rows, cols);
        werase(output_win);

        wattron(output_win, A_BOLD); mvwprintw(output_win, 1, 2, "%s", title); wattroff(output_win, A_BOLD);
        int viewable_lines = rows - 4;
        for (int i = 0; i < viewable_lines; i++) {
            int line_idx = top_line + i;
            if (line_idx < viewer->num_lines) {
                char *line = viewer->lines[line_idx];
                int color_pair = 8;
                if (line[0] == '+') color_pair = 10;
                else if (line[0] == '-') color_pair = 11;
                else if (line[0] == '@' && line[1] == '@') color_pair = 6;
                wattron(output_win, COLOR_PAIR(color_pair));
                mvwprintw(output_win, 3 + i, 2, "%.*s", cols - 2, line);
                wattroff(output_win, COLOR_PAIR(color_pair));
            }
        }
        wattron(output_win, A_REVERSE); mvwprintw(output_win, rows - 2, 2, " Use as SETAS ou PAGE UP/DOWN para rolar | Pressione 'q' ou ESC para sair "); wattroff(output_win, A_REVERSE);
        wrefresh(output_win);
        
        wget_wch(output_win, &ch);
        switch(ch) {
            case KEY_UP: if (top_line > 0) top_line--; break;
            case KEY_DOWN: if (top_line < viewer->num_lines - viewable_lines) top_line++; break;
            case KEY_PPAGE: top_line -= viewable_lines; if (top_line < 0) top_line = 0; break;
            case KEY_NPAGE: top_line += viewable_lines; if (top_line >= viewer->num_lines) top_line = viewer->num_lines - 1; break;
            case KEY_SR: top_line -= PAGE_JUMP; if (top_line < 0) top_line = 0; break;
            case KEY_SF: if (top_line < viewer->num_lines - viewable_lines) { top_line += PAGE_JUMP; if (top_line > viewer->num_lines - viewable_lines) top_line = viewer->num_lines - viewable_lines; } break;
            case 'q': case 27: goto end_viewer;
        }
    }
    end_viewer:
    delwin(output_win);
    destroy_file_viewer(viewer);
    if(filename) remove(filename);
}

FileViewer* create_file_viewer(const char* filename) {
    FILE *f = fopen(filename, "r");
    if (!f) return NULL;
    FileViewer *viewer = malloc(sizeof(FileViewer));
    if (!viewer) { fclose(f); return NULL; }
    viewer->lines = NULL; viewer->num_lines = 0;
    char line_buffer[MAX_LINE_LEN];
    while (fgets(line_buffer, sizeof(line_buffer), f)) {
        viewer->num_lines++;
        viewer->lines = realloc(viewer->lines, sizeof(char*) * viewer->num_lines);
        line_buffer[strcspn(line_buffer, "\n")] = 0;
        viewer->lines[viewer->num_lines - 1] = strdup(line_buffer);
    }
    fclose(f);
    return viewer;
}

void destroy_file_viewer(FileViewer* viewer) {
    if (!viewer) return;
    for (int i = 0; i < viewer->num_lines; i++) free(viewer->lines[i]);
    free(viewer->lines);
    free(viewer);
}

// ===================================================================
// 7. Text Editing & Manipulation
// ===================================================================

void editor_handle_enter(EditorState *state) {
    push_undo(state);
    clear_redo_stack(state);
    if (state->num_lines >= MAX_LINES) return;
    char *current_line_ptr = state->lines[state->current_line];
    if (!current_line_ptr) return;

    int base_indent_len = 0;
    while (current_line_ptr[base_indent_len] != '\0' && isspace(current_line_ptr[base_indent_len])) {
        base_indent_len++;
    }

    int extra_indent = 0;
    if (state->auto_indent_on_newline && !state->paste_mode) {
        int last_char_pos = state->current_col - 1;
        while (last_char_pos >= 0 && isspace(current_line_ptr[last_char_pos])) {
            last_char_pos--;
        }
        if (last_char_pos >= 0 && current_line_ptr[last_char_pos] == '{') {
            extra_indent = TAB_SIZE;
        }
    }

    int new_indent_len = base_indent_len + extra_indent;
    if (state->paste_mode) new_indent_len = 0;

    int line_len = strlen(current_line_ptr);
    int col = state->current_col;
    if (col > line_len) col = line_len;
    char *rest_of_line = &current_line_ptr[col];

    int rest_len = strlen(rest_of_line);
    char *new_line_content = malloc(new_indent_len + rest_len + 1);
    if (!new_line_content) return;
    for (int i = 0; i < new_indent_len; i++) new_line_content[i] = ' ';
    strcpy(new_line_content + new_indent_len, rest_of_line);

    current_line_ptr[col] = '\0';
    char* resized_line = realloc(current_line_ptr, col + 1);
    if (resized_line) state->lines[state->current_line] = resized_line;

    for (int i = state->num_lines; i > state->current_line + 1; i--) {
        state->lines[i] = state->lines[i - 1];
    }
    state->num_lines++;
    state->lines[state->current_line + 1] = new_line_content;

    state->current_line++;
    state->current_col = new_indent_len;
    state->ideal_col = new_indent_len;
    state->buffer_modified = true;
}

void editor_handle_backspace(EditorState *state) {
    push_undo(state);
    clear_redo_stack(state);
    if (state->current_col == 0 && state->current_line == 0) return;
    if (state->current_col > 0) {
        char *line = state->lines[state->current_line];
        if (!line) return;
        int line_len = strlen(line);

        int prev_char_start = state->current_col - 1;
        while (prev_char_start > 0 && (line[prev_char_start] & 0xC0) == 0x80) {
            prev_char_start--;
        }

        memmove(&line[prev_char_start], &line[state->current_col], line_len - state->current_col + 1);
        char* resized_line = realloc(line, line_len - (state->current_col - prev_char_start) + 1);
        if (resized_line) state->lines[state->current_line] = resized_line;
        
        state->current_col = prev_char_start;
        state->ideal_col = state->current_col;
    } else { 
        if (state->current_line == 0) return;
        int prev_line_idx = state->current_line - 1;
        char *prev_line = state->lines[prev_line_idx]; 
        char *current_line_ptr = state->lines[state->current_line];
        if (!prev_line || !current_line_ptr) return;
        
        int prev_len = strlen(prev_line);
        int current_len = strlen(current_line_ptr);
        
        char *new_prev_line = realloc(prev_line, prev_len + current_len + 1); 
        if (!new_prev_line) return; 
        
        memcpy(new_prev_line + prev_len, current_line_ptr, current_len + 1);
        state->lines[prev_line_idx] = new_prev_line; 
        
        free(current_line_ptr);
        for (int i = state->current_line; i < state->num_lines - 1; i++) {
            state->lines[i] = state->lines[i + 1];
        }
        state->num_lines--; 
        state->lines[state->num_lines] = NULL; 
        
        state->current_line--; 
        state->current_col = prev_len; 
        state->ideal_col = state->current_col;
    }
    state->buffer_modified = true;
}

void editor_insert_char(EditorState *state, wint_t ch) {
    push_undo(state);
    clear_redo_stack(state);
    if (state->current_line >= state->num_lines) return;
    char *line = state->lines[state->current_line];
    if (!line) { line = calloc(1, 1); if (!line) return; state->lines[state->current_line] = line; }
    int line_len = strlen(line);
    
    char multibyte_char[MB_CUR_MAX + 1];
    int char_len = wctomb(multibyte_char, ch); if (char_len < 0) return;
    multibyte_char[char_len] = '\0';

    if (line_len + char_len >= MAX_LINE_LEN - 1) return;
    char *new_line = realloc(line, line_len + char_len + 1); if (!new_line) return;
    state->lines[state->current_line] = new_line;

    if (state->current_col < line_len) {
        memmove(&new_line[state->current_col + char_len], &new_line[state->current_col], line_len - state->current_col);
    }
    memcpy(&new_line[state->current_col], multibyte_char, char_len);
    state->current_col += char_len; 
    state->ideal_col = state->current_col;
    new_line[line_len + char_len] = '\0';
    state->buffer_modified = true;
}

void editor_delete_line(EditorState *state) {
    push_undo(state);
    clear_redo_stack(state);
    if (state->num_lines <= 1 && state->current_line == 0) {
        free(state->lines[0]);
        state->lines[0] = calloc(1, 1);
        state->current_col = 0; state->ideal_col = 0;
        return;
    }
    free(state->lines[state->current_line]);
    for (int i = state->current_line; i < state->num_lines - 1; i++) {
        state->lines[i] = state->lines[i + 1];
    }
    state->num_lines--;
    state->lines[state->num_lines] = NULL;
    if (state->current_line >= state->num_lines) {
        state->current_line = state->num_lines - 1;
    }
    state->current_col = 0; state->ideal_col = 0;
    state->buffer_modified = true;
}

char* trim_whitespace(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;

    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    return str;
}

// ===================================================================
// 8. Cursor Movement & Navigation
// ===================================================================

void ensure_cursor_in_bounds(EditorState *state) {
    if (state->num_lines == 0) { state->current_line = 0; state->current_col = 0; return; }
    if (state->current_line >= state->num_lines) state->current_line = state->num_lines - 1;
    if (state->current_line < 0) state->current_line = 0;
    char *line = state->lines[state->current_line];
    int line_len = line ? strlen(line) : 0;
    if (state->current_col > line_len) state->current_col = line_len;
    if (state->current_col < 0) state->current_col = 0;
}

void editor_move_to_next_word(EditorState *state) {
    char *line = state->lines[state->current_line]; if (!line) return;
    int len = strlen(line);
    while (state->current_col < len && isspace(line[state->current_col])) state->current_col++;
    while (state->current_col < len && !isspace(line[state->current_col])) state->current_col++;
    state->ideal_col = state->current_col;
}

void editor_move_to_previous_word(EditorState *state) {
    char *line = state->lines[state->current_line]; if (!line || state->current_col == 0) return;
    while (state->current_col > 0 && isspace(line[state->current_col - 1])) state->current_col--;
    while (state->current_col > 0 && !isspace(line[state->current_col - 1])) state->current_col--;
    state->ideal_col = state->current_col;
}

// ===================================================================
// 9. Search
// ===================================================================

void editor_find(EditorState *state) {
    JanelaEditor *active_jw = gerenciador.janelas[gerenciador.janela_ativa_idx];
    WINDOW *win = active_jw->win;
    int rows, cols;
    getmaxyx(win, rows, cols);
    
    char search_term[100];
    snprintf(state->command_buffer, sizeof(state->command_buffer), "/");
    state->command_pos = 1;
    
    
    while (1) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Search: %s", state->command_buffer + 1);
        editor_redraw(win, state);
        
        wint_t ch;
        wget_wch(win, &ch);
                
        if (ch == KEY_ENTER || '\n') {
            strncpy(search_term, state->command_buffer + 1, sizeof(search_term - 1));
            search_term[sizeof(search_term) - 1] = '\0';
            break;
        } else if (ch == 27) {
            search_term[0] = '\0';
            break;
        } else if (ch == KEY_BACKSPACE || ch == 27 || ch == 8) {
            if (state->command_pos > 1) {
                state->command_pos--;
                state->command_buffer[state->command_pos] = '\0';
            }
        } else if (isprint(ch) && state->command_pos < sizeof(state->command_buffer) - 1) {
            state->command_buffer[state->command_pos++] = ch;
            state->command_buffer[state->command_pos] = '\0';
        }
        
    }
    
    state->status_msg[0] = '\0';
    state->command_buffer[0] = '\0';
    redesenhar_todas_as_janelas();
    
    if (strlen(search_term) == 0) {
        return;
    }
    
    strncpy(state->last_search, search_term, sizeof(state->last_search) - 1);
    state->last_match_line = state->current_line;
    state->last_match_col = state->current_col;
    
    for (int i = 0; i < state->num_lines; i++) {
        int line_num = (state->current_line + i + 1) % state->num_lines;
        char *line = state->lines[line_num];
        char *match = strstr(line, search_term);
        if (match) {
            state->current_line = line_num;
            state->current_col = match - line;
            state->ideal_col = state->current_col;
            snprintf(state->status_msg, sizeof(state->status_msg), "Find in %d:%d", state->current_line + 1, state->current_col + 1);
            return;
       }
       
    }
    snprintf(state->status_msg, sizeof(state->status_msg), "Word wasn't found: %s", search_term);

}

void editor_find_next(EditorState *state) {
    if (state->last_search[0] == '\0') {
        snprintf(state->status_msg, sizeof(state->status_msg), "Nenhum termo para buscar. Use Ctrl+F primeiro.");
        return;
    }

    int start_line = state->current_line, start_col = state->current_col + 1;
    for (int i = 0; i < state->num_lines; i++) {
        int current_line_idx = (start_line + i) % state->num_lines;
        char *line = state->lines[current_line_idx];
        if (i > 0) start_col = 0;
        char *match = strstr(&line[start_col], state->last_search); 
        if (match) {
            state->current_line = current_line_idx;
            state->current_col = match - line;
            state->ideal_col = state->current_col;
            snprintf(state->status_msg, sizeof(state->status_msg), "Encontrado em L:%d C:%d", state->current_line + 1, state->current_col + 1);
            return;
        }
    }
    snprintf(state->status_msg, sizeof(state->status_msg), "Nenhuma outra ocorrência de: %s", state->last_search);
}

void editor_find_previous(EditorState *state) {
    if (state->last_search[0] == '\0') {
        snprintf(state->status_msg, sizeof(state->status_msg), "Nenhum termo para buscar. Use Ctrl+F primeiro.");
        return;
    }

    int start_line = state->current_line, start_col = state->current_col;
    for (int i = 0; i < state->num_lines; i++) {
        int current_line_idx = (start_line - i + state->num_lines) % state->num_lines;
        char *line = state->lines[current_line_idx];
        char *last_match_in_line = NULL;
        char *match = strstr(line, state->last_search);
        while (match) {
            if (current_line_idx == start_line && (match - line) >= start_col) break;
            last_match_in_line = match;
            match = strstr(match + 1, state->last_search);
        }
        if (last_match_in_line) {
            state->current_line = current_line_idx;
            state->current_col = last_match_in_line - line;
            state->ideal_col = state->current_col;
            snprintf(state->status_msg, sizeof(state->status_msg), "Encontrado em L:%d C:%d", state->current_line + 1, state->current_col + 1);
            return;
        }
        start_col = strlen(line);
    }
    snprintf(state->status_msg, sizeof(state->status_msg), "Nenhuma outra ocorrência de: %s", state->last_search);
}

// ===================================================================
// 10. Undo/Redo
// ===================================================================

EditorSnapshot* create_snapshot(EditorState *state) {
    EditorSnapshot *snapshot = malloc(sizeof(EditorSnapshot));
    if (!snapshot) return NULL;
    snapshot->lines = malloc(sizeof(char*) * state->num_lines);
    if (!snapshot->lines) { free(snapshot); return NULL; }
    for (int i = 0; i < state->num_lines; i++) snapshot->lines[i] = strdup(state->lines[i]);
    snapshot->num_lines = state->num_lines;
    snapshot->current_line = state->current_line;
    snapshot->current_col = state->current_col;
    snapshot->ideal_col = state->ideal_col;
    snapshot->top_line = state->top_line;
    snapshot->left_col = state->left_col;
    return snapshot;
}

void free_snapshot(EditorSnapshot *snapshot) {
    if (!snapshot) return;
    for (int i = 0; i < snapshot->num_lines; i++) free(snapshot->lines[i]);
    free(snapshot->lines);
    free(snapshot);
}

void restore_from_snapshot(EditorState *state, EditorSnapshot *snapshot) {
    for (int i = 0; i < state->num_lines; i++) free(state->lines[i]);
    state->num_lines = snapshot->num_lines;
    for (int i = 0; i < state->num_lines; i++) state->lines[i] = snapshot->lines[i];
    state->current_line = snapshot->current_line;
    state->current_col = snapshot->current_col;
    state->ideal_col = snapshot->ideal_col;
    state->top_line = snapshot->top_line;
    state->left_col = snapshot->left_col;
    free(snapshot->lines);
    free(snapshot);
}

void push_undo(EditorState *state) {
    if (state->undo_count >= MAX_UNDO_LEVELS) {
        free_snapshot(state->undo_stack[0]);
        for (int i = 1; i < MAX_UNDO_LEVELS; i++) state->undo_stack[i - 1] = state->undo_stack[i];
        state->undo_count--;
    }
    state->undo_stack[state->undo_count++] = create_snapshot(state);
}

void clear_redo_stack(EditorState *state) {
    for (int i = 0; i < state->redo_count; i++) free_snapshot(state->redo_stack[i]);
    state->redo_count = 0;
}

void do_undo(EditorState *state) {
    if (state->undo_count <= 1) return;
    if (state->redo_count < MAX_UNDO_LEVELS) state->redo_stack[state->redo_count++] = create_snapshot(state);
    EditorSnapshot *undo_snap = state->undo_stack[--state->undo_count];
    restore_from_snapshot(state, undo_snap);
    state->buffer_modified = true;
}

void do_redo(EditorState *state) {
    if (state->redo_count == 0) return;
    EditorSnapshot *redo_snap = state->redo_stack[--state->redo_count];
    push_undo(state);
    restore_from_snapshot(state, redo_snap);
    state->buffer_modified = true;
}

// ===================================================================
// 11. Autocompletion
// ===================================================================

void add_suggestion(EditorState *state, const char *suggestion) {
    for (int i = 0; i < state->num_suggestions; i++) {
        if (strcmp(state->completion_suggestions[i], suggestion) == 0) return;
    }
    state->num_suggestions++;
    state->completion_suggestions = realloc(state->completion_suggestions, state->num_suggestions * sizeof(char*));
    state->completion_suggestions[state->num_suggestions - 1] = strdup(suggestion);
}

void editor_start_completion(EditorState *state) {
    char* line = state->lines[state->current_line];
    if (!line) return;
    int start = state->current_col;
    while (start > 0 && (isalnum(line[start - 1]) || line[start - 1] == '_')) start--;
    state->completion_start_col = start;
    int len = state->current_col - start;
    if (len == 0) return; 

    strncpy(state->word_to_complete, &line[start], len);
    state->word_to_complete[len] = '\0';

    state->num_suggestions = 0;
    state->completion_suggestions = NULL;

    const char *delimiters = " \t\n\r`~!@#$%^&*()-=+[]{}|\\;:'\".,<>/?";
    for (int i = 0; i < state->num_lines; i++) {
        char *line_copy = strdup(state->lines[i]);
        if (!line_copy) continue;
        char *saveptr;
        for (char *token = strtok_r(line_copy, delimiters, &saveptr); token != NULL; token = strtok_r(NULL, delimiters, &saveptr)) {
            if (strncmp(token, state->word_to_complete, len) == 0 && strlen(token) > len) {
                add_suggestion(state, token);
            }
        }
        free(line_copy);
    }
    
    for (int i = 0; i < state->num_lines; i++) {
        for (int j = 0; j < num_known_headers; j++) {
            if (strstr(state->lines[i], known_headers[j].header_name)) {
                for (int k = 0; k < known_headers[j].count; k++) {
                    if (strncmp(known_headers[j].symbols[k], state->word_to_complete, len) == 0) {
                        add_suggestion(state, known_headers[j].symbols[k]);
                    }
                }
            }
        }
    }

    if (state->num_suggestions > 0) {
        state->completion_mode = COMPLETION_TEXT;
        state->selected_suggestion = 0;
        state->completion_scroll_top = 0;
    }
}

void editor_start_command_completion(EditorState *state) {
    if (state->completion_mode != COMPLETION_NONE) return;
    char* buffer = state->command_buffer;
    int len = strlen(buffer);
    if (len == 0) return;

    if (state->completion_suggestions) {
        for (int i = 0; i < state->num_suggestions; i++) free(state->completion_suggestions[i]);
        free(state->completion_suggestions);
        state->completion_suggestions = NULL;
    }
    state->num_suggestions = 0;

    for (int i = 0; i < num_editor_commands; i++) {
        if (strncmp(editor_commands[i], buffer, len) == 0) add_suggestion(state, editor_commands[i]);
    }

    if (state->num_suggestions > 0) {
        state->completion_mode = COMPLETION_COMMAND;
        state->selected_suggestion = 0;
        state->completion_scroll_top = 0;
        strncpy(state->word_to_complete, buffer, sizeof(state->word_to_complete) - 1);
        state->word_to_complete[sizeof(state->word_to_complete) - 1] = '\0';
        state->completion_start_col = 0;
    }
}

void editor_start_file_completion(EditorState *state) {
    char *space = strchr(state->command_buffer, ' ');
    if (!space) return;

    char *prefix = space + 1;
    int prefix_len = strlen(prefix);

    if (state->completion_suggestions) {
        for (int i = 0; i < state->num_suggestions; i++) free(state->completion_suggestions[i]);
        free(state->completion_suggestions);
        state->completion_suggestions = NULL;
    }
    state->num_suggestions = 0;

    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strncmp(dir->d_name, prefix, prefix_len) == 0) add_suggestion(state, dir->d_name);
        }
        closedir(d);
    }

    if (state->num_suggestions > 0) {
        state->completion_mode = COMPLETION_FILE;
        state->selected_suggestion = 0;
        state->completion_scroll_top = 0;
        strncpy(state->word_to_complete, prefix, sizeof(state->word_to_complete) - 1);
        state->word_to_complete[sizeof(state->word_to_complete) - 1] = '\0';
        state->completion_start_col = prefix - state->command_buffer;
    }
}

void editor_end_completion(EditorState *state) {
    state->completion_mode = COMPLETION_NONE;
    if (state->completion_win) { delwin(state->completion_win); state->completion_win = NULL; }
    for (int i = 0; i < state->num_suggestions; i++) free(state->completion_suggestions[i]);
    free(state->completion_suggestions);
    state->completion_suggestions = NULL;
    state->num_suggestions = 0;
    curs_set(1);
}

void editor_apply_completion(EditorState *state) {
    if (state->completion_mode == COMPLETION_NONE || state->num_suggestions == 0) return;

    const char* selected = state->completion_suggestions[state->selected_suggestion];

    if (state->completion_mode == COMPLETION_TEXT) {
        int prefix_len = strlen(state->word_to_complete);
        int selected_len = strlen(selected);
        char* line = state->lines[state->current_line];
        int line_len = strlen(line);
        char* new_line = malloc(line_len - prefix_len + selected_len + 1);
        strncpy(new_line, line, state->completion_start_col);
        strcpy(new_line + state->completion_start_col, selected);
        strcpy(new_line + state->completion_start_col + selected_len, line + state->current_col);
        free(state->lines[state->current_line]);
        state->lines[state->current_line] = new_line;
        state->current_col = state->completion_start_col + selected_len;
        state->ideal_col = state->current_col;
    } else if (state->completion_mode == COMPLETION_COMMAND) {
        strncpy(state->command_buffer, selected, sizeof(state->command_buffer) - 1);
        state->command_buffer[sizeof(state->command_buffer) - 1] = '\0';
        state->command_pos = strlen(state->command_buffer);
        if (strcmp(selected, "q") != 0 && strcmp(selected, "wq") != 0 && strcmp(selected, "new") != 0 && strcmp(selected, "help") != 0) {
            if (state->command_pos < sizeof(state->command_buffer) - 2) {
                state->command_buffer[state->command_pos++] = ' ';
                state->command_buffer[state->command_pos] = '\0';
            }
        }
    } else if (state->completion_mode == COMPLETION_FILE) {
        char *space = strchr(state->command_buffer, ' ');
        if (space) {
            *(space + 1) = '\0';
            strncat(state->command_buffer, selected, sizeof(state->command_buffer) - strlen(state->command_buffer) - 1);
            state->command_pos = strlen(state->command_buffer);
        }
    }

    editor_end_completion(state);
}

void editor_draw_completion_win(WINDOW *win, EditorState *state) {
    int max_len = 0;
    for (int i = 0; i < state->num_suggestions; i++) {
        int len = strlen(state->completion_suggestions[i]);
        if (len > max_len) max_len = len;
    }

    int parent_rows, parent_cols;
    getmaxyx(win, parent_rows, parent_cols);

    int win_h, win_w, win_y, win_x;

    if (state->completion_mode == COMPLETION_TEXT) {
        int visual_cursor_y, visual_cursor_x;
        get_visual_pos(win, state, &visual_cursor_y, &visual_cursor_x);
        
        int cursor_screen_y = visual_cursor_y - state->top_line;
        int max_h = parent_rows - 2 - (cursor_screen_y + 1);
        if (max_h < 3) max_h = 3; if (max_h > 15) max_h = 15;

        win_h = state->num_suggestions < max_h ? state->num_suggestions : max_h;
        win_w = max_len + 2;
        win_y = getbegy(win) + cursor_screen_y + 1;
        win_x = getbegx(win) + get_visual_col(state->lines[state->current_line], state->completion_start_col) % parent_cols;

        if (win_x + win_w >= getbegx(win) + parent_cols) win_x = getbegx(win) + parent_cols - win_w;
        if (win_y < getbegy(win)) win_y = getbegy(win);
        if (win_x < getbegx(win)) win_x = getbegx(win);

    } else if (state->completion_mode == COMPLETION_COMMAND || state->completion_mode == COMPLETION_FILE) {
        int max_h = parent_rows - 2; if (max_h < 3) max_h = 3; if (max_h > 15) max_h = 15;
        win_h = state->num_suggestions < max_h ? state->num_suggestions : max_h;
        win_w = max_len + 2;
        win_y = getbegy(win) + parent_rows - 2 - win_h;
        if (win_y < getbegy(win)) win_y = getbegy(win);
        win_x = getbegx(win) + 1;
    } else {
        return;
    }

    if(state->completion_win) delwin(state->completion_win);
    state->completion_win = newwin(win_h, win_w, win_y, win_x);
    wbkgd(state->completion_win, COLOR_PAIR(9));

    for (int i = 0; i < win_h; i++) {
        int suggestion_idx = state->completion_scroll_top + i;
        if (suggestion_idx < state->num_suggestions) {
            if (suggestion_idx == state->selected_suggestion) wattron(state->completion_win, A_REVERSE);
            mvwprintw(state->completion_win, i, 1, "%.*s", win_w - 2, state->completion_suggestions[suggestion_idx]);
            if (suggestion_idx == state->selected_suggestion) wattroff(state->completion_win, A_REVERSE);
        }
    }

    wnoutrefresh(state->completion_win);
    curs_set(0);
}

// ===================================================================
// 12. Input Handling
// ===================================================================

void handle_insert_mode_key(EditorState *state, wint_t ch) {
    WINDOW *win = gerenciador.janelas[gerenciador.janela_ativa_idx]->win;
    switch (ch) {
        case KEY_CTRL_P: editor_start_completion(state); break;
        case KEY_CTRL_DEL: case KEY_CTRL_K: editor_delete_line(state); break;
        case KEY_CTRL_D: editor_find_next(state); break;
        case KEY_CTRL_A: editor_find_previous(state); break;
        case KEY_CTRL_F: editor_find(state); break;
        case KEY_UNDO: do_undo(state); break;
        case KEY_REDO: do_redo(state); break;
        case KEY_ENTER: case '\n': editor_handle_enter(state); break;
        case KEY_BACKSPACE: case 127: case 8: editor_handle_backspace(state); break;
        case '\t':
            editor_start_completion(state);
            if (state->completion_mode != COMPLETION_TEXT) {
                for (int i = 0; i < TAB_SIZE; i++) editor_insert_char(state, ' ');
            }
            break;
        case KEY_UP: {
            if (state->word_wrap_enabled) {
                int r, cols; getmaxyx(win, r, cols); if (cols <= 0) break;
                state->ideal_col = state->current_col % cols; 
                if (state->current_col >= cols) {
                    state->current_col -= cols;
                } else {
                    if (state->current_line > 0) {
                        state->current_line--;
                        state->current_col = strlen(state->lines[state->current_line]);
                    }
                }
            } else {
                if (state->current_line > 0) state->current_line--;
            }
            break;
        }
        case KEY_DOWN: {
            if (state->word_wrap_enabled) {
                int r, cols; getmaxyx(win, r, cols); if (cols <= 0) break;
                state->ideal_col = state->current_col % cols;
                char *line = state->lines[state->current_line];
                int line_len = strlen(line);
                if (state->current_col + cols < line_len) {
                    state->current_col += cols;
                } else {
                    if (state->current_line < state->num_lines - 1) {
                        state->current_line++;
                        state->current_col = 0;
                    }
                }
            } else {
                if (state->current_line < state->num_lines - 1) state->current_line++;
            }
            break;
        }
        case KEY_LEFT: if (state->current_col > 0) state->current_col--; state->ideal_col = state->current_col; break;
        case KEY_RIGHT: { char* line = state->lines[state->current_line]; int line_len = line ? strlen(line) : 0; if (state->current_col < line_len) state->current_col++; state->ideal_col = state->current_col; } break;
        case KEY_PPAGE: case KEY_SR: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line > 0) state->current_line--; state->current_col = state->ideal_col; break;
        case KEY_NPAGE: case KEY_SF: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line < state->num_lines - 1) state->current_line++; state->current_col = state->ideal_col; break;
        case KEY_HOME: state->current_col = 0; state->ideal_col = 0; break;
        case KEY_END: { char* line = state->lines[state->current_line]; if(line) state->current_col = strlen(line); state->ideal_col = state->current_col; } break;
        case KEY_SDC: editor_delete_line(state); break;
        default: if (iswprint(ch)) { editor_insert_char(state, ch); } break;
    }
}

void handle_command_mode_key(EditorState *state, wint_t ch, bool *should_exit) {
    switch (ch) {
        case KEY_CTRL_P: case '\t':
            if (strncmp(state->command_buffer, "open ", 5) == 0) {
                editor_start_file_completion(state);
            } else {
                editor_start_command_completion(state);
            }
            break;
        case KEY_LEFT: if (state->command_pos > 0) state->command_pos--; break;
        case KEY_RIGHT: if (state->command_pos < strlen(state->command_buffer)) state->command_pos++; break;
        case KEY_UP:
            if (state->history_pos > 0) {
                state->history_pos--;
                strncpy(state->command_buffer, state->command_history[state->history_pos], sizeof(state->command_buffer) - 1);
                state->command_pos = strlen(state->command_buffer);
            }
            break;
        case KEY_DOWN:
            if (state->history_pos < state->history_count) {
                state->history_pos++;
                if (state->history_pos == state->history_count) {
                    state->command_buffer[0] = '\0';
                } else {
                    strncpy(state->command_buffer, state->command_history[state->history_pos], sizeof(state->command_buffer) - 1);
                }
                state->command_pos = strlen(state->command_buffer);
            }
            break;
        case KEY_ENTER: case '\n': process_command(state, should_exit); break;
        case KEY_BACKSPACE: case 127: case 8:
            if (state->command_pos > 0) {
                memmove(&state->command_buffer[state->command_pos - 1], &state->command_buffer[state->command_pos], strlen(state->command_buffer) - state->command_pos + 1);
                state->command_pos--;
            }
            break;
        default:
            if (iswprint(ch) && strlen(state->command_buffer) < sizeof(state->command_buffer) - 1) {
                memmove(&state->command_buffer[state->command_pos + 1], &state->command_buffer[state->command_pos], strlen(state->command_buffer) - state->command_pos + 1);
                state->command_buffer[state->command_pos] = (char)ch;
                state->command_pos++;
            }
            break;
    }
}
