#include "screen_ui.h" // Include its own header
#include "defs.h" // For EditorState, etc.
#include "others.h" // For editor_find_unmatched_brackets
#include "lsp_client.h" // For lsp_draw_diagnostics, get_diagnostic_under_cursor
#include "window_managment.h" // For gerenciador
#include <ctype.h>

// ===================================================================
// Screen & UI
// ===================================================================
WINDOW *draw_pop_up(const char *message, int y, int x) {
    if (!message || !*message) {
        return NULL;
    }
    int max_width = 0;
    int num_lines = 0;
    const char *ptr = message;
    
    while (*ptr) {
        num_lines++;
        const char *line_start = ptr;
        const char *line_end = strchr(ptr, '\n');
        int line_len;
                
        if (line_end) {
            line_len = line_end - line_start;
            ptr = line_end + 1;
       } else {
           line_len = strlen(line_start);
           ptr += line_len;
       }
       if (line_len > max_width) {
           max_width = line_len;
       }
   }        
   
   int win_height = num_lines + 2;
   int win_width = max_width + 4;
   
   int term_rows, term_cols;
   getmaxyx(stdscr, term_rows, term_cols);
   
   if (win_width > term_cols) win_width = term_cols;
   if (win_height > term_cols) win_height = term_cols;
   if (win_width >  term_cols - 2) win_width = term_cols - 2;
   
   if (y + win_height > term_rows) {
       y = term_rows - win_height;
   }
   if (x + win_width > term_cols) {
       x = term_cols;
   }
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   
   WINDOW *popup_win = newwin(win_height, win_width, y, x);
   wbkgd(popup_win, COLOR_PAIR(8));
   box(popup_win, 0, 0);
   
   ptr = message;
   for (int i = 0; i < num_lines; i++) {
       const char *line_start = ptr;
       const char *line_end = strchr(ptr, '\n');
       int line_len;
       
       if (line_end) {
           line_len = line_end - line_start;
           ptr = line_end + 1;
       } else {
           line_len = strlen(line_start);
           ptr += line_len;
       }                    
       
       if (line_len > win_width - 4) {
           line_len = win_width - 4;
       }
       
       mvwprintw(popup_win, i + 1, 2, "%.*s", line_len, line_start);
   }
   return popup_win;
}

void draw_diagnostic_popup(WINDOW *main_win, EditorState *state, const char *message) {
    if (!message || !*message) {
        return;
    }

    int max_width = 0;
    int num_lines = 0;
    const char *ptr = message;

    // Calcula as dimensões da janela
    while (*ptr) {
        num_lines++;
        const char *line_start = ptr;
        const char *line_end = strchr(ptr, '\n');
        int line_len;

        if (line_end) {
            line_len = line_end - line_start;
            ptr = line_end + 1;
        } else {
            line_len = strlen(line_start);
            ptr += line_len;
        }

        if (line_len > max_width) {
            max_width = line_len;
        }
    }

    // Adiciona padding
    int win_height = num_lines + 2;
    int win_width = max_width + 4;

    int term_rows, term_cols;
    getmaxyx(stdscr, term_rows, term_cols);
    if (win_width > term_cols) win_width = term_cols;
    if (win_height > term_rows) win_height = term_rows;
    if (win_width > term_cols - 2) win_width = term_cols - 2;

    // Posiciona a janela
    int win_y, win_x;
    int cursor_y, cursor_x;
    int visual_y, visual_x;
    get_visual_pos(main_win, state, &visual_y, &visual_x);
    
    extern GerenciadorJanelas gerenciador;
    int border_offset = gerenciador.num_janelas > 1 ? 1 : 0;
    cursor_y = (visual_y - state->top_line) + border_offset;
    cursor_x = (visual_x - state->left_col) + border_offset;

    win_y = getbegy(main_win) + cursor_y + 1;
    win_x = getbegx(main_win) + cursor_x;

    if (win_y + win_height > term_rows) {
        win_y = getbegy(main_win) + cursor_y - win_height;
    }
    if (win_x + win_width > term_cols) {
        win_x = term_cols - win_width - 1;
    }
    if (win_x < 0) win_x = 0;
    if (win_y < 0) win_y = 0;

    state->diagnostic_popup = newwin(win_height, win_width, win_y, win_x);
    wbkgd(state->diagnostic_popup, COLOR_PAIR(8));
    box(state->diagnostic_popup, 0, 0);

    // Imprime a mensagem
    ptr = message;
    for (int i = 0; i < num_lines; i++) {
        const char *line_start = ptr;
        const char *line_end = strchr(ptr, '\n');
        int line_len;

        if (line_end) {
            line_len = line_end - line_start;
            ptr = line_end + 1;
        } else {
            line_len = strlen(line_start);
            ptr += line_len;
        }
        
        if (line_len > win_width - 4) {
            line_len = win_width - 4;
        }

        mvwprintw(state->diagnostic_popup, i + 1, 2, "%.*s", line_len, line_start);
    }

    // Marca o popup para atualização, mas não o desenha ainda
    wnoutrefresh(state->diagnostic_popup);
}


void editor_redraw(WINDOW *win, EditorState *state) {
    if (state->diagnostic_popup) {
        delwin(state->diagnostic_popup);
        state->diagnostic_popup = NULL;
    }

    if (state->buffer_modified) {
        editor_find_unmatched_brackets(state);
    }

    werase(win);
    int rows, cols;
    getmaxyx(win, rows, cols);

    extern GerenciadorJanelas gerenciador;
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
                    
                    if (state->lsp_document) {
                        for (int d = 0; d < state->lsp_document->diagnostics_count; d++) {
                            LspDiagnostic *diag = &state->lsp_document->diagnostics[d];
                            if (diag->range.start.line == file_line_idx) {
                                int diag_start_col = diag->range.start.character;
                                int diag_end_col = diag->range.end.character;
                                int segment_start_col = line_offset;
                                int segment_end_col = line_offset + break_pos;

                                if (max(segment_start_col, diag_start_col) < min(segment_end_col, diag_end_col)) {
                                    int y_pos = screen_y + border_offset;
                                    int start_x = border_offset + get_visual_col(line + segment_start_col, max(0, diag_start_col - segment_start_col));
                                    int end_x = border_offset + get_visual_col(line + segment_start_col, min(break_pos, diag_end_col - segment_start_col));
                                    
                                    int color_pair;
                                    switch (diag->severity) {
                                        case LSP_SEVERITY_ERROR: color_pair = 11; break;
                                        case LSP_SEVERITY_WARNING: color_pair = 3; break;
                                        default: color_pair = 8; break;
                                    }

                                    wattron(win, COLOR_PAIR(color_pair));
                                    if (start_x >= 1) mvwaddch(win, y_pos, start_x - 1, '[');
                                    wattroff(win, COLOR_PAIR(color_pair));

                                    mvwchgat(win, y_pos, start_x, end_x - start_x, A_UNDERLINE, color_pair, NULL);

                                    wattron(win, COLOR_PAIR(color_pair));
                                    if (end_x < cols) mvwaddch(win, y_pos, end_x, ']');
                                    wattroff(win, COLOR_PAIR(color_pair));
                                }
                            }
                        }
                    }

                    screen_y++;
                }
                visual_line_idx++;
                line_offset += break_pos;
                if (line_len == 0) break;
            }
        }
    } else { // NO WORD WRAP
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

            if (state->lsp_document) {
                for (int d = 0; d < state->lsp_document->diagnostics_count; d++) {
                    LspDiagnostic *diag = &state->lsp_document->diagnostics[d];
                    if (diag->range.start.line == line_idx) {
                        int y_pos = screen_y + border_offset;
                        int start_x = border_offset + get_visual_col(line, diag->range.start.character) - state->left_col;
                        int end_x = border_offset + get_visual_col(line, diag->range.end.character) - state->left_col;

                        if (start_x < border_offset) start_x = border_offset;
                        if (end_x > cols - border_offset) end_x = cols - border_offset;

                        if (start_x < end_x) {
                            int color_pair;
                            switch (diag->severity) {
                                case LSP_SEVERITY_ERROR: color_pair = 11; break;
                                case LSP_SEVERITY_WARNING: color_pair = 3; break;
                                default: color_pair = 8; break;
                            }

                            wattron(win, COLOR_PAIR(color_pair));
                            if (start_x >= 1 + border_offset) mvwaddch(win, y_pos, start_x - 1, '[');
                            wattroff(win, COLOR_PAIR(color_pair));

                            mvwchgat(win, y_pos, start_x, end_x - start_x, A_UNDERLINE, color_pair, NULL);

                            wattron(win, COLOR_PAIR(color_pair));
                            if (end_x < cols - border_offset) mvwaddch(win, y_pos, end_x, ']');
                            wattroff(win, COLOR_PAIR(color_pair));
                        }
                    }
                }
            }

            screen_y++;
        }
    }

    // Lógica de status e pop-up
    LspDiagnostic *diag = NULL;
    if (state->lsp_enabled) {
        diag = get_diagnostic_under_cursor(state);
        if (diag) {
            snprintf(state->status_msg, STATUS_MSG_LEN, "[%s] %s", diag->code, diag->message);
        }
    }

    // Desenha a barra de status
    int color_pair = 1; // Padrão: azul
    if (strstr(state->status_msg, "Warning:") != NULL || strstr(state->status_msg, "Error:") != NULL) {
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

    // Marca a janela principal (com todo o seu conteúdo) para atualização.
    wnoutrefresh(win);

    // AGORA, se houver um diagnóstico, desenha o pop-up por cima.
    if (diag) {
        draw_diagnostic_popup(win, state, diag->message);
    }

    // Clear the status message for the next redraw cycle
    state->status_msg[0] = '\0';
}



void adjust_viewport(WINDOW *win, EditorState *state) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    
    extern GerenciadorJanelas gerenciador;
    int border_offset = gerenciador.num_janelas > 1 ? 1 : 0;
    int content_height = rows - border_offset - 1;
    int content_width = cols - 2 * border_offset;

    int visual_y, visual_x;
    get_visual_pos(win, state, &visual_y, &visual_x);

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
        if (visual_x >= state->left_col + content_width) {
            state->left_col = visual_x - content_width + 1;
        }
    }
}

void get_visual_pos(WINDOW *win, EditorState *state, int *visual_y, int *visual_x) {
    int rows, cols;
    getmaxyx(win, rows, cols);
    
    extern GerenciadorJanelas gerenciador;
    int border_offset = gerenciador.num_janelas > 1 ? 1 : 0;
    int content_width = cols - 2 * border_offset;

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
                            if (isspace(line[line_offset + j])) {
                                temp_break = j;
                                break;
                            }
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
                    if (isspace(state->lines[state->current_line][current_line_offset + j])) {
                        temp_break = j;
                        break;
                    }
                }
                if (temp_break != -1) break_pos = temp_break + 1;
            }
            current_line_offset += break_pos;
            y++;
        }
        x = get_visual_col(state->lines[state->current_line] + current_line_offset, 
                          state->current_col - current_line_offset);
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
