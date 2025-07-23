#define NCURSES_WIDECHAR 1 // Ativa o suporte a wide-char
#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h> // Para setlocale()
#include <wchar.h>  // Para wint_t

// --- Constantes para Syntax Highlighting ---
#define NUM_C_KEYWORDS 31 // CORRIGIDO
const char *c_keywords[NUM_C_KEYWORDS] = {
    "auto", "break", "case", "char", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "int", "long", "register", "return", "short", "signed", "sizeof", "static",
    "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
};

#define NUM_C_TYPES 10
const char *c_types[NUM_C_TYPES] = {
	"int", "float", "unsigned", "signed", "char", "long", "double",
	"void", "short", "const"
};

#define NUM_C_STD_FUNCTIONS 10
const char *c_std_functions[NUM_C_STD_FUNCTIONS] = {
    "printf", "scanf", "fprintf", "sprintf", "fopen", "fclose", "malloc", "free", "strcpy", "strlen"
};

#define NUM_C_NUM 12
const char *c_numbers[NUM_C_NUM] = {
	"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "<", ">"
};

// --- Definições do Editor ---
#define MAX_LINES 4098
#define MAX_LINE_LEN 2048
#define STATUS_MSG_LEN 250
#define PAGE_JUMP 10


typedef enum {
    NORMAL,
    INSERT,
    COMMAND
} EditorMode;

typedef struct {
    char *lines[MAX_LINES];
    int num_lines;
    int current_line;
    int current_col;
    int ideal_col;
    int top_line;
    int left_col;
    EditorMode mode;
    char filename[256];
    char status_msg[STATUS_MSG_LEN];
    char command_buffer[100];
    int command_pos;
} EditorState;

// --- Declarações de Funções ---
void inicializar_ncurses();
void editor_redraw(EditorState *state);
void load_file(EditorState *state, const char *filename);
void save_file(EditorState *state);
void editor_handle_enter(EditorState *state);
void editor_handle_backspace(EditorState *state);
void editor_insert_char(EditorState *state, wint_t ch);
void process_command(EditorState *state);
void ensure_cursor_in_bounds(EditorState *state);
void adjust_viewport(EditorState *state);


// --- Definições das Funções ---

void inicializar_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
}

void adjust_viewport(EditorState *state) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    rows -= 2;

    if (state->current_line < state->top_line) {
        state->top_line = state->current_line;
    }
    if (state->current_line >= state->top_line + rows) {
        state->top_line = state->current_line - rows + 1;
    }
    if (state->current_col < state->left_col) {
        state->left_col = state->current_col;
    }
    if (state->current_col >= state->left_col + cols) {
        state->left_col = state->current_col - cols + 1;
    }
}

void editor_redraw(EditorState *state) {
    clear();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    adjust_viewport(state);

    for (int i = 0; i < rows - 2; i++) {
        int line_idx = state->top_line + i;
        if (line_idx < state->num_lines) {
            char *line = state->lines[line_idx];
            if (!line) continue;
            move(i, 0);
            clrtoeol();
            int len = strlen(line);
            int start_display_col = state->left_col;
            if (start_display_col >= len) continue;
            char *line_ptr = &line[start_display_col];
            int display_len = len - start_display_col;
            if (display_len > cols) display_len = cols;
            int current_pos = 0;
            while (current_pos < display_len) {
                if (line_ptr[current_pos] == '#' || (line_ptr[current_pos] == '/' && current_pos + 1 < display_len && line_ptr[current_pos + 1] == '/')) {
                    attron(COLOR_PAIR(6));
                    printw("%.*s", display_len - current_pos, &line_ptr[current_pos]);
                    attroff(COLOR_PAIR(6));
                    break;
                }
                int token_start = current_pos;
                if (isalnum(line_ptr[token_start])) {
                    while (current_pos < display_len && isalnum(line_ptr[current_pos])) current_pos++;
                } else {
                    while (current_pos < display_len && !isalnum(line_ptr[current_pos])) {
                        if (line_ptr[current_pos] == '/' && current_pos + 1 < display_len && line_ptr[current_pos + 1] == '/') break;
                        current_pos++;
                    }
                }
                int token_len = current_pos - token_start;
                if (token_len > 0) {
                    char *token_ptr = &line_ptr[token_start];
                    int color_pair = 0;
                    if (isalnum(token_ptr[0])) {
                        for (int j = 0; j < NUM_C_KEYWORDS; j++) {
                            if (strlen(c_keywords[j]) == token_len && strncmp(token_ptr, c_keywords[j], token_len) == 0) {
                                color_pair = 3; break;
                            }
                        }
                        if (!color_pair) {
                            for (int j = 0; j < NUM_C_TYPES; j++) {
                                if (strlen(c_types[j]) == token_len && strncmp(token_ptr, c_types[j], token_len) == 0) {
                                    color_pair = 4; break;
                                }
                            }
                        }
                        if (!color_pair) {
                            for (int j = 0; j < NUM_C_STD_FUNCTIONS; j++) {
                                if (strlen(c_std_functions[j]) == token_len && strncmp(token_ptr, c_std_functions[j], token_len) == 0) {
                                    color_pair = 5; break;
                                }
                            }
                        }
                        if (!color_pair) {
                            for (int j = 0; j < NUM_C_NUM; j++) {
                                if (strlen(c_numbers[j]) == token_len && strncmp(token_ptr, c_numbers[j], token_len) == 0) {
                                    color_pair = 7; // Usa o par de cor 7 que definimos
                                    break;
                                    }
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
    move(rows - 2, 0);
    clrtoeol();
    mvprintw(rows - 2, 0, "%s", state->status_msg);
    attroff(COLOR_PAIR(2));
    attron(COLOR_PAIR(1));
    move(rows - 1, 0);
    clrtoeol();
    if (state->mode == COMMAND) {
        mvprintw(rows - 1, 0, ":%s", state->command_buffer);
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
        mvprintw(rows - 1, 0, "%s | %s | Line %d/%d, Col %d",
                 mode_str, display_filename, state->current_line + 1, state->num_lines, state->current_col + 1);
    }
    attroff(COLOR_PAIR(1));
    if (state->mode == COMMAND) {
        move(rows - 1, strlen(state->command_buffer) + 1);
    } else {
        int cursor_y = state->current_line - state->top_line;
        int cursor_x = state->current_col - state->left_col;
        move(cursor_y, cursor_x);
    }
    curs_set(1);
    refresh();
}

void load_file(EditorState *state, const char *filename) {
    for (int i = 0; i < state->num_lines; i++) {
        if(state->lines[i]) free(state->lines[i]);
        state->lines[i] = NULL;
    }
    state->num_lines = 0;
    strncpy(state->filename, filename, sizeof(state->filename) - 1);
    state->filename[sizeof(state->filename) - 1] = '\0';
    FILE *file = fopen(filename, "r");
    if (file) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), file) && state->num_lines < MAX_LINES) {
            line[strcspn(line, "\n")] = '\0';
            state->lines[state->num_lines] = strdup(line);
            if (!state->lines[state->num_lines]) { fclose(file); return; }
            state->num_lines++;
        }
        fclose(file);
        snprintf(state->status_msg, sizeof(state->status_msg), "\"%s\" loaded", filename);
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
    state->current_line = 0; state->current_col = 0; state->ideal_col = 0; state->top_line = 0; state->left_col = 0;
}

void save_file(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) {
        strncpy(state->status_msg, "No file name. Use :w <filename>", sizeof(state->status_msg) - 1);
        return;
    }
    FILE *file = fopen(state->filename, "w");
    if (file) {
        for (int i = 0; i < state->num_lines; i++) {
            if (state->lines[i]) fprintf(file, "%s\n", state->lines[i]);
        }
        fclose(file);
        char display_filename[40];
        strncpy(display_filename, state->filename, sizeof(display_filename) - 1);
        display_filename[sizeof(display_filename) - 1] = '\0';
        snprintf(state->status_msg, sizeof(state->status_msg), "\"%s\" written", display_filename);
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Error saving: %s", strerror(errno));
    }
}

void editor_handle_enter(EditorState *state) {
    if (state->num_lines >= MAX_LINES) return;
    char *current_line = state->lines[state->current_line];
    if (!current_line) return;
    int line_len = strlen(current_line);
    int col = state->current_col;
    if (col > line_len) col = line_len;
    char *rest_of_line = &current_line[col];
    char *new_line = strdup(rest_of_line);
    if (!new_line) return;
    current_line[col] = '\0';
    char* resized_line = realloc(current_line, col + 1);
    if (resized_line) state->lines[state->current_line] = resized_line;
    for (int i = state->num_lines; i > state->current_line + 1; i--) {
        state->lines[i] = state->lines[i - 1];
    }
    state->num_lines++;
    state->lines[state->current_line + 1] = new_line;
    state->current_line++;
    state->current_col = 0;
    state->ideal_col = 0;
}

void editor_handle_backspace(EditorState *state) {
    if (state->current_col == 0 && state->current_line == 0) return;
    if (state->current_col > 0) {
        editor_insert_char(state, '\b');
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
}

void editor_insert_char(EditorState *state, wint_t ch) {
    if (state->current_line >= state->num_lines) return;
    char *line = state->lines[state->current_line];
    if (!line) {
        line = calloc(1, 1);
        if (!line) return;
        state->lines[state->current_line] = line;
    }
    int line_len = strlen(line);

    if (ch == '\b') { // Lógica de backspace dentro de uma linha
        if (state->current_col > 0) {
            memmove(&line[state->current_col - 1], &line[state->current_col], line_len - state->current_col + 1);
            state->current_col--;
            state->ideal_col = state->current_col;
        }
        return;
    }

    char multibyte_char[MB_CUR_MAX + 1];
    int char_len = wctomb(multibyte_char, ch);
    if (char_len < 0) return;
    multibyte_char[char_len] = '\0';
    if (line_len + char_len >= MAX_LINE_LEN - 1) return;
    char *new_line = realloc(line, line_len + char_len + 1);
    if (!new_line) return;
    state->lines[state->current_line] = new_line;
    if (state->current_col < line_len) {
        memmove(&new_line[state->current_col + char_len], &new_line[state->current_col], line_len - state->current_col);
    }
    memcpy(&new_line[state->current_col], multibyte_char, char_len);
    state->current_col += char_len;
    state->ideal_col = state->current_col;
    new_line[line_len + char_len] = '\0';
}

void process_command(EditorState *state) {
    char command[100];
    char args[100] = "";
    sscanf(state->command_buffer, "%s %s", command, args);
    if (strcmp(command, "q") == 0) {
        endwin();
        exit(0);
    } else if (strcmp(command, "w") == 0) {
        if (strlen(args) > 0) {
            strncpy(state->filename, args, sizeof(state->filename) - 1);
        }
        save_file(state);
    } else if (strcmp(command, "open") == 0) {
        if (strlen(args) > 0) {
            load_file(state, args);
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "Usage: :open <filename>");
        }
    } else if (strcmp(command, "new") == 0) {
        for (int i = 0; i < state->num_lines; i++) {
            if(state->lines[i]) free(state->lines[i]);
            state->lines[i] = NULL;
        }
        state->num_lines = 1;
        state->lines[0] = calloc(1, 1);
        strcpy(state->filename, "[No Name]");
        state->current_line = 0; state->current_col = 0; state->ideal_col = 0; state->top_line = 0; state->left_col = 0;
        snprintf(state->status_msg, sizeof(state->status_msg), "New file opened.");
    } else if (strcmp(command, "wq") == 0) {
        save_file(state);
        endwin();
        exit(0);
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Unknown command: %s", command);
    }
    state->mode = NORMAL;
}

void ensure_cursor_in_bounds(EditorState *state) {
    if (state->num_lines == 0) {
        state->current_line = 0;
        state->current_col = 0;
        return;
    }
    if (state->current_line >= state->num_lines) {
        state->current_line = state->num_lines - 1;
    }
    if (state->current_line < 0) {
        state->current_line = 0;
    }
    char *line = state->lines[state->current_line];
    int line_len = line ? strlen(line) : 0;
    if (state->current_col > line_len) {
        state->current_col = line_len;
    }
    if (state->current_col < 0) {
        state->current_col = 0;
    }
}

// Função main() atualizada com a funcionalidade de rolagem rápida

int main(int argc, char *argv[]) {
    setlocale(LC_ALL, "");
    inicializar_ncurses();
    EditorState *state = calloc(1, sizeof(EditorState));
    if (!state) {
        endwin();
        fprintf(stderr, "Fatal: Could not allocate memory for editor state.\n");
        return 1;
    }
    state->mode = NORMAL;
    strcpy(state->filename, "[No Name]");
    if (argc > 1) {
        load_file(state, argv[1]);
    } else {
        state->lines[0] = calloc(1, 1);
        if (!state->lines[0]) {
            endwin();
            free(state);
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        state->num_lines = 1;
    }
    while (1) {
        ensure_cursor_in_bounds(state);
        editor_redraw(state);
        wint_t ch;
        int result = get_wch(&ch);
        if (result != ERR) {
            if (state->mode != COMMAND) {
                state->status_msg[0] = '\0';
            }
            switch (state->mode) {
                case NORMAL:
                    switch (ch) {
                        case 'i': state->mode = INSERT; break;
                        case ':': state->mode = COMMAND; state->command_buffer[0] = '\0'; state->command_pos = 0; break;
                        case KEY_UP: if (state->current_line > 0) { state->current_line--; state->current_col = state->ideal_col; } break;
                        case KEY_DOWN: if (state->current_line < state->num_lines - 1) { state->current_line++; state->current_col = state->ideal_col; } break;
                        case KEY_LEFT: if (state->current_col > 0) state->current_col--; state->ideal_col = state->current_col; break;
                        case KEY_RIGHT: { char* line = state->lines[state->current_line]; int line_len = line ? strlen(line) : 0; if (state->current_col < line_len) state->current_col++; state->ideal_col = state->current_col; } break;
                        
                        // --- NOVA FUNCIONALIDADE AQUI (MODO NORMAL) ---
                        case KEY_PPAGE: // Page Up
                        case KEY_SR:    // Shift + Seta para Cima
                            for (int i = 0; i < PAGE_JUMP; i++) {
                                if (state->current_line > 0) {
                                    state->current_line--;
                                }
                            }
                            state->current_col = state->ideal_col;
                            break;
                        case KEY_NPAGE: // Page Down
                        case KEY_SF:    // Shift + Seta para Baixo
                            for (int i = 0; i < PAGE_JUMP; i++) {
                                if (state->current_line < state->num_lines - 1) {
                                    state->current_line++;
                                }
                            }
                            state->current_col = state->ideal_col;
                            break;
                        // --- FIM DA NOVA FUNCIONALIDADE ---
                    }
                    break;
                case INSERT:
                    switch (ch) {
                        case 27: state->mode = NORMAL; break; 
                        case KEY_ENTER: case '\n': editor_handle_enter(state); break;
                        case KEY_BACKSPACE: case 127: case 8: editor_handle_backspace(state); break;
                        case '\t': for (int i = 0; i < TABSIZE; i++) { editor_insert_char(state, ' '); } break;
                        case KEY_UP: if (state->current_line > 0) { state->current_line--; state->current_col = state->ideal_col; } break;
                        case KEY_DOWN: if (state->current_line < state->num_lines - 1) { state->current_line++; state->current_col = state->ideal_col; } break;
                        case KEY_LEFT: if (state->current_col > 0) state->current_col--; state->ideal_col = state->current_col; break;
                        case KEY_RIGHT: { char* line = state->lines[state->current_line]; int line_len = line ? strlen(line) : 0; if (state->current_col < line_len) state->current_col++; state->ideal_col = state->current_col; } break;

                        // --- NOVA FUNCIONALIDADE AQUI (MODO INSERT) ---
                        case KEY_PPAGE: // Page Up
                        case KEY_SR:    // Shift + Seta para Cima
                            for (int i = 0; i < PAGE_JUMP; i++) {
                                if (state->current_line > 0) {
                                    state->current_line--;
                                }
                            }
                            state->current_col = state->ideal_col;
                            break;
                        case KEY_NPAGE: // Page Down
                        case KEY_SF:    // Shift + Seta para Baixo
                            for (int i = 0; i < PAGE_JUMP; i++) {
                                if (state->current_line < state->num_lines - 1) {
                                    state->current_line++;
                                }
                            }
                            state->current_col = state->ideal_col;
                            break;
                        // --- FIM DA NOVA FUNCIONALIDADE ---

                        default: if (ch >= 32 && ch != 127) { editor_insert_char(state, ch); } break;
                    }
                    break;
                case COMMAND:
                     if (ch == 27) { state->mode = NORMAL; state->status_msg[0] = '\0';
                    } else if (ch == '\n' || ch == KEY_ENTER) { process_command(state);
                    } else if ((ch == KEY_BACKSPACE || ch == 127) && state->command_pos > 0) { state->command_buffer[--state->command_pos] = '\0';
                    } else if (ch >= 32 && ch < 127 && state->command_pos < sizeof(state->command_buffer) - 1) {
                        state->command_buffer[state->command_pos++] = (char)ch;
                        state->command_buffer[state->command_pos] = '\0';
                    }
                    break;
            }
        }
    }
    endwin();
    free(state);
    return 0;
}

