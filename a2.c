#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINES 1000
#define MAX_LINE_LEN 1024

// Editor modes
typedef enum {
    NORMAL,
    INSERT,
    COMMAND
} EditorMode;

// Structure for the editor state
typedef struct {
    char *lines[MAX_LINES];
    int num_lines;
    int current_line;
    int current_col;
    int ideal_col;
    int top_line;
    EditorMode mode;
    char filename[256];
    char status_msg[100];
    char command_buffer[100];
} EditorState;

// Forward declarations
void save_file(EditorState *state);
void load_file(EditorState *state, const char *filename);

void inicializar_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
}

void editor_redraw(EditorState *state) {
    clear();
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // Draw text lines
    for (int i = 0; i < rows - 2; i++) {
        int line_idx = state->top_line + i;
        if (line_idx < state->num_lines) {
            mvprintw(i, 0, "%s", state->lines[line_idx]);
        }
    }

    // Status message line (above the bottom bar)
    attron(COLOR_PAIR(2));
    move(rows - 2, 0);
    clrtoeol();
    mvprintw(rows - 2, 0, "%s", state->status_msg);
    attroff(COLOR_PAIR(2));

    // Bottom bar (mode or command)
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
        mvprintw(rows - 1, 0, "%s | %s | Line %d/%d, Col %d",
                 mode_str, state->filename, state->current_line + 1, state->num_lines, state->current_col + 1);
    }
    attroff(COLOR_PAIR(1));

    // Position cursor
    if (state->mode == COMMAND) {
        move(rows - 1, strlen(state->command_buffer) + 1);
    } else {
        move(state->current_line - state->top_line, state->current_col);
    }

    curs_set(state->mode == INSERT || state->mode == COMMAND);
    refresh();
}

void load_file(EditorState *state, const char *filename) {
    // Clear existing content
    for (int i = 0; i < state->num_lines; i++) {
        free(state->lines[i]);
        state->lines[i] = NULL;
    }
    state->num_lines = 0;

    strncpy(state->filename, filename, sizeof(state->filename) - 1);
    state->filename[sizeof(state->filename) - 1] = '\0';

    FILE *file = fopen(filename, "r");
    if (file) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), file) && state->num_lines < MAX_LINES) {
            line[strcspn(line, "\n")] = 0;
            state->lines[state->num_lines++] = strdup(line);
        }
        fclose(file);
        snprintf(state->status_msg, sizeof(state->status_msg), "\"%s\" loaded", filename);
    }
    if (state->num_lines == 0) {
        state->lines[0] = calloc(1, 1);
        state->num_lines = 1;
        if (file) { // File was empty
             snprintf(state->status_msg, sizeof(state->status_msg), "\"%s\" is an empty file", filename);
        } else { // File did not exist
             snprintf(state->status_msg, sizeof(state->status_msg), "New file: \"%s\"", filename);
        }
    }
}

void save_file(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "No file name. Use :w <filename>");
        return;
    }
    FILE *file = fopen(state->filename, "w");
    if (file) {
        for (int i = 0; i < state->num_lines; i++) {
            fprintf(file, "%s\n", state->lines[i]);
        }
        fclose(file);
        snprintf(state->status_msg, sizeof(state->status_msg), "\"%s\" written", state->filename);
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Error saving to \"%s\"", state->filename);
    }
}

void editor_handle_enter(EditorState *state) {
    if (state->num_lines >= MAX_LINES) return;
    char *current_line_content = state->lines[state->current_line];
    char *rest_of_line = &current_line_content[state->current_col];
    char *new_line = strdup(rest_of_line);
    current_line_content[state->current_col] = '\0';
    for (int i = state->num_lines; i > state->current_line + 1; i--) {
        state->lines[i] = state->lines[i - 1];
    }
    state->lines[state->current_line + 1] = new_line;
    state->num_lines++;
    state->current_line++;
    state->current_col = 0;
    state->ideal_col = 0;
}

void editor_handle_backspace(EditorState *state) {
    if (state->current_col > 0) {
        char *line = state->lines[state->current_line];
        memmove(&line[state->current_col - 1], &line[state->current_col], strlen(line) - state->current_col + 1);
        state->current_col--;
    } else if (state->current_line > 0) {
        char *prev_line = state->lines[state->current_line - 1];
        char *current_line = state->lines[state->current_line];
        int prev_len = strlen(prev_line);
        char *new_prev_line = realloc(prev_line, prev_len + strlen(current_line) + 1);
        if (!new_prev_line) return;
        strcpy(&new_prev_line[prev_len], current_line);
        state->lines[state->current_line - 1] = new_prev_line;
        free(current_line);
        for (int i = state->current_line; i < state->num_lines - 1; i++) {
            state->lines[i] = state->lines[i + 1];
        }
        state->num_lines--;
        state->current_line--;
        state->current_col = prev_len;
    }
    state->ideal_col = state->current_col;
}

void editor_insert_char(EditorState *state, int ch) {
    char *line = state->lines[state->current_line];
    int len = strlen(line);
    if (len >= MAX_LINE_LEN - 1) return;
    char *new_line = realloc(line, len + 2);
    if (!new_line) return;
    state->lines[state->current_line] = new_line;
    memmove(&new_line[state->current_col + 1], &new_line[state->current_col], len - state->current_col + 1);
    new_line[state->current_col] = ch;
    state->current_col++;
    state->ideal_col = state->current_col;
}

void process_command(EditorState *state) {
    char *command = state->command_buffer;
    char *args = strchr(command, ' ');
    if (args) {
        *args = '\0'; // Split command and arguments
        args++;
    }

    if (strcmp(command, "q") == 0) {
        endwin();
        exit(0);
    } else if (strcmp(command, "w") == 0) {
        if (args) { // :w <filename>
            strncpy(state->filename, args, sizeof(state->filename) - 1);
            state->filename[sizeof(state->filename) - 1] = '\0';
        }
        save_file(state);
    } else if (strcmp(command, "open") == 0) {
        if (args) {
            load_file(state, args);
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "Usage: :open <filename>");
        }
    } else if (strcmp(command, "new") == 0) {
        for (int i = 0; i < state->num_lines; i++) {
            free(state->lines[i]);
        }
        state->num_lines = 1;
        state->lines[0] = calloc(1, 1);
        strcpy(state->filename, "[No Name]");
        snprintf(state->status_msg, sizeof(state->status_msg), "New file opened.");
    } else if (strcmp(command, "help") == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Commands: :q, :w, :open, :new, :help");
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Unknown command: %s", command);
    }
    state->mode = NORMAL;
}

void ensure_cursor_in_bounds(EditorState *state) {
    int line_len = strlen(state->lines[state->current_line]);
    if (state->current_col > line_len) {
        state->current_col = line_len;
    }
}

int main(int argc, char *argv[]) {
    inicializar_ncurses();
    EditorState state = {0};
    state.mode = NORMAL;

    if (argc > 1) {
        load_file(&state, argv[1]);
    } else {
        state.lines[0] = calloc(1, 1);
        state.num_lines = 1;
        strcpy(state.filename, "[No Name]");
    }

    while (1) {
        ensure_cursor_in_bounds(&state);
        editor_redraw(&state);
        int ch = getch();

        // Clear one-time status messages on any keypress, unless we are entering command mode
        if (ch != ':' && state.mode != COMMAND) {
            strcpy(state.status_msg, "");
        }

        switch (state.mode) {
            case NORMAL:
                switch (ch) {
                    case 'i': state.mode = INSERT; break;
                    case ':':
                        state.mode = COMMAND;
                        strcpy(state.command_buffer, "");
                        break;
                    case KEY_UP: if (state.current_line > 0) state.current_line--; break;
                    case KEY_DOWN: if (state.current_line < state.num_lines - 1) state.current_line++; break;
                    case KEY_LEFT: if (state.current_col > 0) state.current_col--; break;
                    case KEY_RIGHT: state.current_col++; break; // Can go past end of line
                }
                break;
            case INSERT:
                switch (ch) {
                    case 27: state.mode = NORMAL; break; // ESC
                    case KEY_ENTER: case '\n': editor_handle_enter(&state); break;
                    case KEY_BACKSPACE: case 127: editor_handle_backspace(&state); break;
                    default:
                        if (isprint(ch)) editor_insert_char(&state, ch);
                        break;
                }
                break;
            case COMMAND:
                if (ch == 27) { // ESC
                    state.mode = NORMAL;
                    strcpy(state.status_msg, "");
                } else if (ch == '\n' || ch == KEY_ENTER) {
                    process_command(&state);
                } else if ((ch == KEY_BACKSPACE || ch == 127) && strlen(state.command_buffer) > 0) {
                    state.command_buffer[strlen(state.command_buffer) - 1] = '\0';
                } else if (isprint(ch)) {
                    strncat(state.command_buffer, (char*)&ch, 1);
                }
                break;
        }
    }

    endwin();
    return 0;
}