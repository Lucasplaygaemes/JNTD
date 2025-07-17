#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINES 1000
#define MAX_LINE_LEN 1024

// Estrutura para o editor
typedef struct {
    char *lines[MAX_LINES];
    int num_lines;
    int current_line;
    int current_col;
    int ideal_col; // Coluna "ideal" para movimentação vertical
    int top_line; // Linha do texto no topo da tela
} EditorState;

void inicializar_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);
    start_color();
    clear();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
}

void editor_redraw(EditorState *state, const char* status_msg) {
    clear();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Desenha as linhas de texto
    for (int i = 0; i < rows - 1; i++) {
        int line_idx = state->top_line + i;
        if (line_idx < state->num_lines) {
            mvprintw(i, 0, "%s", state->lines[line_idx]);
        }
    }

    // Barra de status
    attron(COLOR_PAIR(1));
    mvprintw(rows - 1, 0, "%.*s", cols, status_msg);
    // Preenche o resto da barra de status
    for (int i = strlen(status_msg); i < cols; i++) {
        addch(' ');
    }
    attroff(COLOR_PAIR(1));

    // Reposiciona o cursor
    move(state->current_line - state->top_line, state->current_col);
    refresh();
}

void save_file(const char *filename, EditorState *state) {
    FILE *file = fopen(filename, "w");
    if (file != NULL) {
        for (int i = 0; i < state->num_lines; i++) {
            fprintf(file, "%s\n", state->lines[i]);
        }
        fclose(file);
    }
}

void editor_handle_enter(EditorState *state) {
    if (state->num_lines >= MAX_LINES) return; // Limite de linhas

    char *current_line_content = state->lines[state->current_line];
    char *rest_of_line = &current_line_content[state->current_col];

    // Aloca memória para a nova linha
    char *new_line = strdup(rest_of_line);
    if (!new_line) return;

    // Trunca a linha atual
    current_line_content[state->current_col] = '\0';

    // Move as linhas para baixo para abrir espaço
    for (int i = state->num_lines; i > state->current_line + 1; i--) {
        state->lines[i] = state->lines[i - 1];
    }

    // Insere a nova linha
    state->lines[state->current_line + 1] = new_line;
    state->num_lines++;
    state->current_line++;
    state->current_col = 0;
}

void editor_handle_backspace(EditorState *state) {
    if (state->current_col > 0) {
        // Apaga caractere na mesma linha
        char *line = state->lines[state->current_line];
        memmove(&line[state->current_col - 1], &line[state->current_col], strlen(line) - state->current_col + 1);
        state->current_col--;
    } else if (state->current_line > 0) {
        // Junta com a linha anterior
        char *prev_line = state->lines[state->current_line - 1];
        char *current_line = state->lines[state->current_line];
        int prev_len = strlen(prev_line);

        // Realoca a linha anterior para caber a atual
        char *new_prev_line = realloc(prev_line, prev_len + strlen(current_line) + 1);
        if (!new_prev_line) return;
        
        strcpy(&new_prev_line[prev_len], current_line);
        state->lines[state->current_line - 1] = new_prev_line;

        // Libera a memória da linha atual e move as outras para cima
        free(current_line);
        for (int i = state->current_line; i < state->num_lines - 1; i++) {
            state->lines[i] = state->lines[i + 1];
        }
        state->num_lines--;
        state->current_line--;
        state->current_col = prev_len;
    }
}

void editor_insert_char(EditorState *state, int ch) {
    char *line = state->lines[state->current_line];
    int len = strlen(line);
    if (len >= MAX_LINE_LEN - 1) return;

    // Realoca para mais um caractere
    char *new_line = realloc(line, len + 2);
    if (!new_line) return;
    state->lines[state->current_line] = new_line;

    // Move o resto da linha para a direita
    memmove(&new_line[state->current_col + 1], &new_line[state->current_col], len - state->current_col + 1);
    new_line[state->current_col] = ch;
    state->current_col++;
}

int modo_visual() {
	printf("Modo visual.\n");
}

int editor_mode() {
    curs_set(1); // Garante que o cursor esteja visível no modo editor
    EditorState state = {0};
    state.num_lines = 1;
    state.lines[0] = calloc(1, 1); // Começa com uma linha vazia
    state.ideal_col = 0;

    int ch;
    char status_msg[100] = "Modo editor... (Pressione ESC para salvar e sair)";

    while ((ch = getch()) != 27) { // 27 é o código ASCII para ESC
        switch (ch) {
            case KEY_UP:
                if (state.current_line > 0) {
                    state.current_line--;
                    state.current_col = state.ideal_col;
                }
                break;
            case KEY_DOWN:
                if (state.current_line < state.num_lines - 1) {
                    state.current_line++;
                    state.current_col = state.ideal_col;
                }
                break;
            case KEY_LEFT:
                if (state.current_col > 0) {
                    state.current_col--;
                    state.ideal_col = state.current_col;
                }
                break;
            case KEY_RIGHT:
                if (state.current_col < strlen(state.lines[state.current_line])) {
                    state.current_col++;
                    state.ideal_col = state.current_col;
                }
                break;
            case KEY_ENTER:
            case '\n':
                editor_handle_enter(&state);
                state.ideal_col = state.current_col; // Manter a indentação
                break;
            case KEY_BACKSPACE:
            case 127:
                editor_handle_backspace(&state);
                state.ideal_col = state.current_col;
                break;
            default:
                if (isprint(ch)) {
                    editor_insert_char(&state, ch);
                    state.ideal_col = state.current_col;
                }
                break;
        }

        // Garante que o cursor não vá além do final da linha
        int line_len = strlen(state.lines[state.current_line]);
        if (state.current_col > line_len) {
            state.current_col = line_len;
        }

        // Lógica de scroll
        int rows, cols;
        getmaxyx(stdscr, rows, cols);
        if (state.current_line < state.top_line) {
            state.top_line = state.current_line;
        }
        if (state.current_line >= state.top_line + rows - 1) {
            state.top_line = state.current_line - (rows - 1) + 1;
        }

        editor_redraw(&state, status_msg);
    }

    // Salvar arquivo
    char filename[256] = {0};
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    
    echo();
    mvprintw(rows - 1, 0, "Nome do arquivo para salvar: ");
    getstr(filename);
    noecho();

    if (strlen(filename) > 0) {
        save_file(filename, &state);
        mvprintw(rows - 1, 0, "Arquivo salvo como '%s'. Pressione qualquer tecla para sair.", filename);
    } else {
        mvprintw(rows - 1, 0, "Salvamento cancelado. Pressione qualquer tecla para sair.");
    }
    
    getch();

    // Libera memória
    for (int i = 0; i < state.num_lines; i++) {
        free(state.lines[i]);
    }

    return 0;
}

int main() {
    inicializar_ncurses();
    curs_set(0);
    printw("Bem vindo ao A2 o editor de texto do JNTD.\n");
    printw("Pressione 'e' para entrar no modo editor.\n");
    refresh();

    int choice = getch();
    if (choice == 'e') {
        editor_mode();
    } else if (choice == 'v') {
	    modo_visual();
    }
    endwin();
    return 0;
}

