#define NCURSES_WIDECHAR 1
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

#define KEY_CTRL_P 16

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

// --- Constantes para Syntax Highlighting ---
#define NUM_C_KEYWORDS 31
const char *c_keywords[NUM_C_KEYWORDS] = {
    "auto", "break", "case", "char", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
};
#define NUM_C_TYPES 10
const char *c_types[NUM_C_TYPES] = { "int", "float", "unsigned", "signed", "char", "long", "double", "void", "short", "const" };
#define NUM_C_STD_FUNCTIONS 10
const char *c_std_functions[NUM_C_STD_FUNCTIONS] = { "printf", "scanf", "fprintf", "sprintf", "fopen", "fclose", "malloc", "free", "strcpy", "strlen" };

// --- Dicionários estáticos para autocompletar headers ---
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

// --- Definições do Editor ---
#define MAX_LINES 4098
#define MAX_LINE_LEN 2048
#define STATUS_MSG_LEN 250
#define PAGE_JUMP 10
#define TAB_SIZE 4
#define MAX_COMMAND_HISTORY 50 

typedef enum { NORMAL, INSERT, COMMAND } EditorMode;
typedef struct {
    char *lines[MAX_LINES];
    int num_lines, current_line, current_col, ideal_col, top_line, left_col, command_pos;
    EditorMode mode;
    char filename[256], status_msg[STATUS_MSG_LEN], command_buffer[100];
    char *command_history[MAX_COMMAND_HISTORY];
    int history_count;
    int history_pos;
    //New struct to plugins//
    char word_to_complete[100];
    int completion_start_col;
    int completion_scroll_top;
    // Campos para o estado do autocompletar
    bool completion_active;
    char **completion_suggestions;
    int num_suggestions;
    int selected_suggestion;
    WINDOW *completion_win;
    char word_to_complete[100];
    int completion_start_col;
    int completion_scroll_top;// +++ NOVO: Para controlar a rolagem da lista
    SyntaxRule *syntax_rules;      // Array dinâmico para as regras de cor
    int num_syntax_rules;

    char **dictionary_words;       // (Vamos usar este no futuro para o autocompletar)
    int num_dictionary_words;
} EditorState;

// Nova função para carregar regras de sintaxe de um arquivo
void load_syntax_file(EditorState *state, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        // Se o arquivo não existir, não fazemos nada. O editor funcionará sem cores.
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        // Ignora comentários e linhas em branco
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        // Remove a quebra de linha do final
        line[strcspn(line, "\r\n")] = 0;

        // Separa a linha em "TIPO" e "palavra" usando o ":" como delimitador
        char *type_str = strtok(line, ":");
        char *word_str = strtok(NULL, ":");

        if (type_str && word_str) {
            // Aumenta o tamanho do nosso array de regras
            state->num_syntax_rules++;
            state->syntax_rules = realloc(state->syntax_rules, sizeof(SyntaxRule) * state->num_syntax_rules);
            
            SyntaxRule *new_rule = &state->syntax_rules[state->num_syntax_rules - 1];
            new_rule->word = strdup(word_str);

            // Converte o TIPO de string para nosso enum
            if (strcmp(type_str, "KEYWORD") == 0) {
                new_rule->type = SYNTAX_KEYWORD;
            } else if (strcmp(type_str, "TYPE") == 0) {
                new_rule->type = SYNTAX_TYPE;
            } else if (strcmp(type_str, "STD_FUNCTION") == 0) {
                new_rule->type = SYNTAX_STD_FUNCTION;
            }
        }
    }
    fclose(file);
}

// --- Declarações de Funções ---
void inicializar_ncurses();
void display_help_screen();
void display_output_screen(const char *title, const char *filename);
void compile_file(EditorState *state, char* args);
void execute_shell_command(EditorState *state);
void add_to_command_history(EditorState *state, const char* command);
void editor_redraw(EditorState *state);
void load_file(EditorState *state, const char *filename);
void save_file(EditorState *state);
void editor_handle_enter(EditorState *state);
void editor_handle_backspace(EditorState *state);
void editor_insert_char(EditorState *state, wint_t ch);
void editor_delete_line(EditorState *state);
void editor_move_to_next_word(EditorState *state);
void editor_move_to_previous_word(EditorState *state);
void process_command(EditorState *state);
void ensure_cursor_in_bounds(EditorState *state);
void adjust_viewport(EditorState *state);
void handle_insert_mode_key(EditorState *state, wint_t ch);


// Declarações de Funções do Autocompletar
void editor_start_completion(EditorState *state);
void editor_end_completion(EditorState *state);
void editor_draw_completion_win(EditorState *state);
void editor_apply_completion(EditorState *state);
void add_suggestion(EditorState *state, const char *suggestion);


// --- Definições das Funções ---

void inicializar_ncurses() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE); init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK); init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK); init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(8, COLOR_WHITE, COLOR_BLACK);
    init_pair(9, COLOR_WHITE, COLOR_MAGENTA);
    bkgd(COLOR_PAIR(8)); 
}

void adjust_viewport(EditorState *state) {
    int rows, cols; getmaxyx(stdscr, rows, cols); rows -= 2;
    if (state->current_line < state->top_line) state->top_line = state->current_line;
    if (state->current_line >= state->top_line + rows) state->top_line = state->current_line - rows + 1;
    if (state->current_col < state->left_col) state->left_col = state->current_col;
    if (state->current_col >= state->left_col + cols) state->left_col = state->current_col - cols + 1;
}

void display_help_screen() {
    static const CommandInfo commands[] = {
        {":w", "Salva o arquivo atual."}, {":w <nome>", "Salva com um novo nome."},
        {":q", "Sai do editor."}, {":wq", "Salva e sai."}, {":open <nome>", "Abre um arquivo."},
        {":new", "Cria um novo arquivo em branco."}, {":help", "Mostra esta tela de ajuda."},
        {":gcc [libs]", "Compila o arquivo atual (ex: :gcc -lm)."},
        {"![cmd]", "Executa um comando do shell (ex: !ls -l)."}
    };
    int num_commands = sizeof(commands) / sizeof(commands[0]);
    
    attron(COLOR_PAIR(8));
    clear(); 
    bkgd(COLOR_PAIR(8));

    attron(A_BOLD); mvprintw(2, 2, "--- AJUDA DO EDITOR ---"); attroff(A_BOLD);
    for (int i = 0; i < num_commands; i++) {
        mvprintw(4 + i, 4, "%-15s: %s", commands[i].command, commands[i].description);
    }
    attron(A_REVERSE); mvprintw(6 + num_commands, 2, " Pressione qualquer tecla para voltar ao editor "); attroff(A_REVERSE);
    refresh(); get_wch(NULL);

    bkgd(COLOR_PAIR(8));
    attroff(COLOR_PAIR(8));
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

void display_output_screen(const char *title, const char *filename) {
    FileViewer *viewer = create_file_viewer(filename);
    if (!viewer) { if(filename) remove(filename); return; }
    int top_line = 0;
    wint_t ch;
    while (1) {
        int rows, cols; getmaxyx(stdscr, rows, cols);
        attron(COLOR_PAIR(8));
        clear();
        bkgd(COLOR_PAIR(8));

        attron(A_BOLD); mvprintw(1, 2, "%s", title); attroff(A_BOLD);
        int viewable_lines = rows - 4;
        for (int i = 0; i < viewable_lines; i++) {
            int line_idx = top_line + i;
            if (line_idx < viewer->num_lines) {
                mvprintw(3 + i, 2, "%.*s", cols - 2, viewer->lines[line_idx]);
            }
        }
        attron(A_REVERSE); mvprintw(rows - 2, 2, " Use as SETAS ou PAGE UP/DOWN para rolar | Pressione 'q' ou ESC para sair "); attroff(A_REVERSE);
        refresh();
        get_wch(&ch);
        switch(ch) {
            case KEY_UP: if (top_line > 0) top_line--; break;
            case KEY_DOWN: if (top_line < viewer->num_lines - viewable_lines) top_line++; break;
            case KEY_PPAGE: top_line -= viewable_lines; if (top_line < 0) top_line = 0; break;
            case KEY_NPAGE: top_line += viewable_lines; if (top_line >= viewer->num_lines) top_line = viewer->num_lines - 1; break;
            case 'q': case 27: goto end_viewer;
        }
    }
end_viewer:
    destroy_file_viewer(viewer);
    if(filename) remove(filename);
    bkgd(COLOR_PAIR(8));
    attroff(COLOR_PAIR(8));
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

void editor_redraw(EditorState *state) {
    erase();

    int rows, cols; 
    getmaxyx(stdscr, rows, cols); 
    adjust_viewport(state);

    for (int i = 0; i < rows - 2; i++) {
        int line_idx = state->top_line + i;
        if (line_idx < state->num_lines) {
            char *line = state->lines[line_idx]; 
            if (!line) continue;
            
            move(i, 0); 
            int len = strlen(line); 
            int start_display_col = state->left_col;
            if (start_display_col >= len) continue;
            char *line_ptr = &line[start_display_col]; 
            int display_len = len - start_display_col;
            if (display_len > cols) display_len = cols;
            int current_pos = 0;
            while (current_pos < display_len) {
                if (line_ptr[current_pos] == '#' || (line_ptr[current_pos] == '/' && current_pos + 1 < display_len && line_ptr[current_pos + 1] == '/')) {
                    attron(COLOR_PAIR(6)); printw("%.*s", display_len - current_pos, &line_ptr[current_pos]); attroff(COLOR_PAIR(6)); break;
                }
                int token_start = current_pos;
                if (isalnum(line_ptr[token_start]) || line_ptr[token_start] == '_') { 
                    while (current_pos < display_len && (isalnum(line_ptr[current_pos]) || line_ptr[current_pos] == '_')) current_pos++;
                } else {
                    while (current_pos < display_len && !isalnum(line_ptr[current_pos]) && line_ptr[current_pos] != '_') {
                        if (line_ptr[current_pos] == '/' && current_pos + 1 < display_len && line_ptr[current_pos + 1] == '/') break;
                        current_pos++;
                    }
                }
                int token_len = current_pos - token_start;
                if (token_len > 0) {
                    char *token_ptr = &line_ptr[token_start]; int color_pair = 0;
                    if (isalnum(token_ptr[0]) || token_ptr[0] == '_') {
    // Agora temos apenas UM loop que verifica nossa lista de regras carregadas
    		for (int j = 0; j < state->num_syntax_rules; j++) {
        	if (strlen(state->syntax_rules[j].word) == token_len && 
            	strncmp(token_ptr, state->syntax_rules[j].word, token_len) == 0) {

            // Mapeia o tipo da regra para a cor correta
            	switch(state->syntax_rules[j].type) {
                case SYNTAX_KEYWORD:      color_pair = 3; break; // Amarelo
                case SYNTAX_TYPE:         color_pair = 4; break; // Verde
                case SYNTAX_STD_FUNCTION: color_pair = 5; break; // Azul
            }
            break; // Encontrou a regra, pode parar de procurar
        }
    }
}
if (color_pair) attron(COLOR_PAIR(color_pair));
                    printw("%.*s", token_len, token_ptr);
                    if (color_pair) attroff(COLOR_PAIR(color_pair));
                }
            }
        }
    }

    attron(COLOR_PAIR(2)); 
    mvprintw(rows - 2, 0, "%s", state->status_msg); 
    attroff(COLOR_PAIR(2));
    attron(COLOR_PAIR(1)); 
    move(rows - 1, 0); 
    clrtoeol();
    if (state->mode == COMMAND) {
        mvprintw(rows - 1, 0, ":%s", state->command_buffer);
    } else {
        char mode_str[20];
        switch (state->mode) { case NORMAL: strcpy(mode_str, "-- NORMAL --"); break; case INSERT: strcpy(mode_str, "-- INSERT --"); break; default: strcpy(mode_str, "--          --"); break; }
        char display_filename[40]; strncpy(display_filename, state->filename, sizeof(display_filename) - 1); display_filename[sizeof(display_filename) - 1] = '\0'; 
        mvprintw(rows - 1, 0, "%s | %s | Line %d/%d, Col %d", mode_str, display_filename, state->current_line + 1, state->num_lines, state->current_col + 1);
    }
    attroff(COLOR_PAIR(1)); 
    
    if (state->mode == COMMAND) {
        move(rows - 1, state->command_pos + 1);
    } else {
        move(state->current_line - state->top_line, state->current_col - state->left_col);
    }

    wnoutrefresh(stdscr);

    if (state->completion_active) {
        editor_draw_completion_win(state);
    } else {
       curs_set(1);
    }

    doupdate();
}

void load_file(EditorState *state, const char *filename) {
    for (int i = 0; i < state->num_lines; i++) { if(state->lines[i]) free(state->lines[i]); state->lines[i] = NULL; }
    state->num_lines = 0; strncpy(state->filename, filename, sizeof(state->filename) - 1); state->filename[sizeof(state->filename) - 1] = '\0';
    FILE *file = fopen(filename, "r");
    if (file) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), file) && state->num_lines < MAX_LINES) {
            line[strcspn(line, "\n")] = '\0'; state->lines[state->num_lines] = strdup(line);
            if (!state->lines[state->num_lines]) { fclose(file); return; }
            state->num_lines++;
        }
        fclose(file); snprintf(state->status_msg, sizeof(state->status_msg), "\"%s\" loaded", filename);
    } else {
        if (errno == ENOENT) {
            state->lines[0] = calloc(1, 1); if (!state->lines[0]) return;
            state->num_lines = 1; snprintf(state->status_msg, sizeof(state->status_msg), "New file: \"%s\"", filename);
        } else { snprintf(state->status_msg, sizeof(state->status_msg), "Error opening file: %s", strerror(errno)); }
    }
    if (state->num_lines == 0) { state->lines[0] = calloc(1, 1); state->num_lines = 1; }
    state->current_line = 0; state->current_col = 0; state->ideal_col = 0; state->top_line = 0; state->left_col = 0;
}

void save_file(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) { strncpy(state->status_msg, "No file name. Use :w <filename>", sizeof(state->status_msg) - 1); return; }
    FILE *file = fopen(state->filename, "w");
    if (file) {
        for (int i = 0; i < state->num_lines; i++) if (state->lines[i]) fprintf(file, "%s\n", state->lines[i]);
        fclose(file); char display_filename[40]; strncpy(display_filename, state->filename, sizeof(display_filename) - 1); display_filename[sizeof(display_filename) - 1] = '\0';
        snprintf(state->status_msg, sizeof(state->status_msg), "\"%s\" written", display_filename);
    } else { snprintf(state->status_msg, sizeof(state->status_msg), "Error saving: %s", strerror(errno)); }
}

void editor_handle_enter(EditorState *state) {
    if (state->num_lines >= MAX_LINES) return;
    char *current_line = state->lines[state->current_line]; if (!current_line) return;
    int line_len = strlen(current_line); int col = state->current_col; if (col > line_len) col = line_len;
    char *rest_of_line = &current_line[col]; char *new_line = strdup(rest_of_line); if (!new_line) return;
    current_line[col] = '\0';
    char* resized_line = realloc(current_line, col + 1); if (resized_line) state->lines[state->current_line] = resized_line;
    for (int i = state->num_lines; i > state->current_line + 1; i--) state->lines[i] = state->lines[i - 1];
    state->num_lines++; state->lines[state->current_line + 1] = new_line;
    state->current_line++; state->current_col = 0; state->ideal_col = 0;
}

void editor_handle_backspace(EditorState *state) {
    if (state->current_col == 0 && state->current_line == 0) return;
    if (state->current_col > 0) {
        char *line = state->lines[state->current_line];
        int line_len = strlen(line);
        memmove(&line[state->current_col - 1], &line[state->current_col], line_len - state->current_col + 1);
        state->current_col--; 
        state->ideal_col = state->current_col;
    } else {
        if (state->current_line == 0) return;
        int prev_line_idx = state->current_line - 1;
        char *prev_line = state->lines[prev_line_idx]; char *current_line_ptr = state->lines[state->current_line];
        if (!prev_line || !current_line_ptr) return;
        int prev_len = strlen(prev_line); int current_len = strlen(current_line_ptr);
        char *new_prev_line = realloc(prev_line, prev_len + current_len + 1); if (!new_prev_line) return;
        memcpy(new_prev_line + prev_len, current_line_ptr, current_len + 1);
        state->lines[prev_line_idx] = new_prev_line;
        free(current_line_ptr);
        for (int i = state->current_line; i < state->num_lines - 1; i++) state->lines[i] = state->lines[i + 1];
        state->num_lines--; state->lines[state->num_lines] = NULL;
        state->current_line--; state->current_col = prev_len; state->ideal_col = state->current_col;
    }
}

void editor_insert_char(EditorState *state, wint_t ch) {
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
}

void editor_delete_line(EditorState *state) {
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

void process_command(EditorState *state) {
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
    strncpy(args, buffer_ptr, sizeof(args) - 1);
    args[sizeof(args)-1] = '\0';
    if (strcmp(command, "q") == 0) { endwin(); exit(0); } 
    else if (strcmp(command, "w") == 0) {
        if (strlen(args) > 0) strncpy(state->filename, args, sizeof(state->filename) - 1);
        save_file(state);
    } else if (strcmp(command, "help") == 0) {
        display_help_screen();
    } else if (strcmp(command, "gcc") == 0) {
        compile_file(state, args);
    } else if (strcmp(command, "open") == 0) {
        if (strlen(args) > 0) load_file(state, args);
        else snprintf(state->status_msg, sizeof(state->status_msg), "Usage: :open <filename>");
    } else if (strcmp(command, "new") == 0) {
        for (int i = 0; i < state->num_lines; i++) { if(state->lines[i]) free(state->lines[i]); state->lines[i] = NULL; }
        state->num_lines = 1; state->lines[0] = calloc(1, 1); strcpy(state->filename, "[No Name]");
        state->current_line = 0; state->current_col = 0; state->ideal_col = 0; state->top_line = 0; state->left_col = 0;
        snprintf(state->status_msg, sizeof(state->status_msg), "New file opened.");
    } else if (strcmp(command, "wq") == 0) { save_file(state); endwin(); exit(0); } 
    else { snprintf(state->status_msg, sizeof(state->status_msg), "Unknown command: %s", command); }
    state->mode = NORMAL;
}

void ensure_cursor_in_bounds(EditorState *state) {
    if (state->num_lines == 0) { state->current_line = 0; state->current_col = 0; return; }
    if (state->current_line >= state->num_lines) state->current_line = state->num_lines - 1;
    if (state->current_line < 0) state->current_line = 0;
    char *line = state->lines[state->current_line];
    int line_len = line ? strlen(line) : 0;
    if (state->current_col > line_len) state->current_col = line_len;
    if (state->current_col < 0) state->current_col = 0;
}

// --- Funções do Autocompletar ---

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
    while (start > 0 && (isalnum(line[start - 1]) || line[start - 1] == '_')) {
        start--;
    }
    state->completion_start_col = start;
    int len = state->current_col - start;
    if (len == 0) return; 

    strncpy(state->word_to_complete, &line[start], len);
    state->word_to_complete[len] = '\0';

    state->num_suggestions = 0;
    state->completion_suggestions = NULL;

    const char *delimiters = " \t\n\r`~!@#$%^&*()-=+[]{}|\\;:'\",.<>/?";
    for (int i = 0; i < state->num_lines; i++) {
        char *line_copy = strdup(state->lines[i]);
        if (!line_copy) continue;
        char *saveptr;
        for (char *token = strtok_r(line_copy, delimiters, &saveptr);
             token != NULL;
             token = strtok_r(NULL, delimiters, &saveptr))
        {
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
        state->completion_active = true;
        state->selected_suggestion = 0;
        state->completion_scroll_top = 0; // +++ NOVO: Reseta a rolagem
    }
}

void editor_end_completion(EditorState *state) {
    state->completion_active = false;
    if (state->completion_win) {
        delwin(state->completion_win);
        state->completion_win = NULL;
    }
    for (int i = 0; i < state->num_suggestions; i++) {
        free(state->completion_suggestions[i]);
    }
    free(state->completion_suggestions);
    state->completion_suggestions = NULL;
    state->num_suggestions = 0;
    curs_set(1); 
}

void editor_apply_completion(EditorState *state) {
    if (!state->completion_active || state->num_suggestions == 0) return;

    const char* selected = state->completion_suggestions[state->selected_suggestion];
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

    editor_end_completion(state);
}

// +++ FUNÇÃO COMPLETAMENTE REFEITA +++
void editor_draw_completion_win(EditorState *state) {
    int max_len = 0;
    for (int i = 0; i < state->num_suggestions; i++) {
        int len = strlen(state->completion_suggestions[i]);
        if (len > max_len) max_len = len;
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Calcula a altura máxima que a janela pode ter sem sair da tela
    int cursor_screen_y = state->current_line - state->top_line;
    int max_h = rows - 2 - (cursor_screen_y + 1); // Espaço abaixo do cursor
    if (max_h < 3) max_h = 3; // Mínimo de 3 linhas
    if (max_h > 15) max_h = 15; // Máximo de 15 linhas

    // A altura da janela é o menor valor entre o número de sugestões e a altura máxima
    int win_h = state->num_suggestions < max_h ? state->num_suggestions : max_h;
    int win_w = max_len + 2; 
    
    int win_y = cursor_screen_y + 1;
    int win_x = state->completion_start_col - state->left_col;
    
    if (win_x + win_w >= cols) win_x = cols - win_w;
    if (win_y < 0) win_y = 0;
    if (win_x < 0) win_x = 0;

    if(state->completion_win) delwin(state->completion_win); 
    state->completion_win = newwin(win_h, win_w, win_y, win_x);
    wbkgd(state->completion_win, COLOR_PAIR(9)); 

    // Desenha apenas os itens visíveis na janela, baseado na rolagem
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

void handle_insert_mode_key(EditorState *state, wint_t ch) {
    switch (ch) {
        case KEY_CTRL_P:
            editor_start_completion(state);
            break;
        case KEY_ENTER: case '\n': editor_handle_enter(state); break;
        case KEY_BACKSPACE: case 127: case 8: editor_handle_backspace(state); break;
        case '\t': for (int i = 0; i < TAB_SIZE; i++) editor_insert_char(state, ' '); break;
        case KEY_UP: if (state->current_line > 0) { state->current_line--; state->current_col = state->ideal_col; } break;
        case KEY_DOWN: if (state->current_line < state->num_lines - 1) { state->current_line++; state->current_col = state->ideal_col; } break;
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

int main(int argc, char *argv[]) {
    #define _XOPEN_SOURCE_EXTENDED 1
    #define NCURSES_WIDECHAR 1
    
    setlocale(LC_ALL, ""); 
    inicializar_ncurses();
    EditorState *state = calloc(1, sizeof(EditorState));
    if (!state) { endwin(); fprintf(stderr, "Fatal: Could not allocate memory for editor state.\n"); return 1; }
    state->mode = NORMAL; 
    strcpy(state->filename, "[No Name]");
    state->completion_active = false; 
    state->completion_win = NULL;
    load_syntax_file(state, "c.syntax");
    if (argc > 1) { 
        load_file(state, argv[1]);
    } else {
        state->lines[0] = calloc(1, 1);
        if (!state->lines[0]) { endwin(); free(state); fprintf(stderr, "Memory allocation failed\n"); return 1; }
        state->num_lines = 1;
    }

    while (1) {
        ensure_cursor_in_bounds(state);
        editor_redraw(state);
        wint_t ch;
        get_wch(&ch);

        if (state->completion_active) {
            switch(ch) {
                case KEY_UP:
                    state->selected_suggestion = (state->selected_suggestion - 1 + state->num_suggestions) % state->num_suggestions;
                    if (state->selected_suggestion < state->completion_scroll_top) {
                        state->completion_scroll_top = state->selected_suggestion;
                    }
                    break;
                case KEY_DOWN:
                    {
                        state->selected_suggestion = (state->selected_suggestion + 1) % state->num_suggestions;
                        int win_h = 0;
                        if(state->completion_win) win_h = getmaxy(state->completion_win);
                        if(win_h > 0 && state->selected_suggestion >= state->completion_scroll_top + win_h) {
                            state->completion_scroll_top++;
                        }
                        if(state->selected_suggestion == 0) state->completion_scroll_top = 0;
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
                    if (state->mode == INSERT) handle_insert_mode_key(state, ch);
                    break;
            }
            continue; 
        }
            
        if (ch == 27) { // Tecla ESC ou uma combinação com Alt
            nodelay(stdscr, TRUE);
            int next_ch = getch();
            nodelay(stdscr, FALSE);

            if (next_ch == ERR) { // Apenas a tecla ESC foi pressionada
                if (state->mode == INSERT) state->mode = NORMAL;
            } else if (next_ch == 'f' || next_ch == 'w') { // Alt+f ou Alt+w
                editor_move_to_next_word(state);
            } else if (next_ch == 'b' || next_ch == 'q') { // Alt+b ou Alt+q
                editor_move_to_previous_word(state);
            }
            continue; // Pula o resto do processamento de teclas desta iteração
        }

        switch (state->mode) {
            case NORMAL:
                switch (ch) {
                    case 'i': state->mode = INSERT; break;
                    case ':': state->mode = COMMAND; state->history_pos = state->history_count; state->command_buffer[0] = '\0'; state->command_pos = 0; break;
                    case KEY_UP: if (state->current_line > 0) { state->current_line--; state->current_col = state->ideal_col; } break;
                    case KEY_DOWN: if (state->current_line < state->num_lines - 1) { state->current_line++; state->current_col = state->ideal_col; } break;
                    case KEY_LEFT: if (state->current_col > 0) state->current_col--; state->ideal_col = state->current_col; break;
                    case KEY_RIGHT: { char* line = state->lines[state->current_line]; int line_len = line ? strlen(line) : 0; if (state->current_col < line_len) state->current_col++; state->ideal_col = state->current_col; } break;
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
                switch (ch) {
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
                    case KEY_ENTER: case '\n': process_command(state); break;
                    case KEY_BACKSPACE: case 127:
                        if (state->command_pos > 0) {
                            memmove(&state->command_buffer[state->command_pos - 1], &state->command_buffer[state->command_pos], strlen(state->command_buffer) - state->command_pos + 1);
                            state->command_pos--;
                        }
                        break;
                    default:
                        if (ch >= 32 && ch < 127 && strlen(state->command_buffer) < sizeof(state->command_buffer) - 1) {
                            memmove(&state->command_buffer[state->command_pos + 1], &state->command_buffer[state->command_pos], strlen(state->command_buffer) - state->command_pos + 1);
                            state->command_buffer[state->command_pos] = (char)ch;
                            state->command_pos++;
                        }
                        break;
                }
                break;
        }
    }
    if (state->completion_active) editor_end_completion(state);
    for(int i=0; i < state->history_count; i++) free(state->command_history[i]);

    // +++ ADICIONE ESTE BLOCO PARA LIMPAR A MEMÓRIA DA SINTAXE +++
    for (int i = 0; i < state->num_syntax_rules; i++) {
        free(state->syntax_rules[i].word);
    }
    free(state->syntax_rules);

    free(state); 
    return 0;
}

}
