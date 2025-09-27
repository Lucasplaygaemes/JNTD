#include "defs.h"
#include "screen_ui.h"
#include "window_managment.h"
#include "fileio.h"
#include "direct_navigation.h"
#include "command_execution.h"
#include "others.h"
#include "lsp_client.h"
#include "timer.h"
#include <locale.h>
#include <libgen.h> // For dirname()
#include <limits.h> // For PATH_MAX

// Global variable definition
// GerenciadorJanelas gerenciador; // Definido em defs.c

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
    init_pair(12, COLOR_BLACK, COLOR_YELLOW);
    bkgd(COLOR_PAIR(8));
}

int main(int argc, char *argv[]) {
    // Find and store the executable's directory path
    char exe_path_buf[PATH_MAX];
    if (realpath(argv[0], exe_path_buf)) {
        char* dir = dirname(exe_path_buf);
        if (dir) {
            strncpy(executable_dir, dir, PATH_MAX - 1);
            executable_dir[PATH_MAX - 1] = '\0';
        }
    }

    EditorState state;
    memset(&state, 0, sizeof(EditorState));
    start_work_timer();
    setlocale(LC_ALL, "");
    inicializar_ncurses();
    
    inicializar_gerenciador_janelas();

    if (argc > 1) {
        criar_nova_janela(argv[1]);
        EditorState *state = gerenciador.janelas[0]->estado;
    
        napms(100);
        
        lsp_initialize(state);
        // Esperar um pouco pela inicialização antes de didOpen
        napms(500);

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
        

        if (state->lsp_enabled) {
            lsp_process_messages(state);
        }
        redesenhar_todas_as_janelas();
        wint_t ch;
        wget_wch(active_win, &ch);
        
        static int check_counter = 0;
        if (check_counter++ % 10 == 0) {
            if (state->lsp_enabled && !lsp_process_alive(state)) {
                snprintf(state->status_msg, STATUS_MSG_LEN, "LSP terminou inesperadamente");
                state->lsp_enabled = false;
            }
        }

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
                case ' ':
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
                 if (state->mode == INSERT || state->mode == VISUAL) {
                    state->mode = NORMAL;
                }
                 if (state->is_moving) {
                     state->is_moving = false;
                     free(state->move_register);
                     state->move_register = NULL;
                     snprintf(state->status_msg, sizeof(state->status_msg), "Move cancelled.");
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
                } else if (next_ch == '.' || next_ch == '>') {
                    ciclar_layout();
                } else if (next_ch >= '1' && next_ch <= '9') {
                    int target_pos = next_ch - '1';
                    mover_janela_para_posicao(target_pos);
                } else if (next_ch == 'r' || next_ch == 'R') {
                    rotacionar_janelas();
                }
            }
            continue;
        }

        switch (state->mode) {
            case VISUAL:
                switch (ch) {
                    case 25: // Ctrl+Y
                        if (state->visual_selection_mode == VISUAL_MODE_NONE) {
                            state->selection_start_line = state->current_line;
                            state->selection_start_col = state->current_col;
                            state->visual_selection_mode = VISUAL_MODE_YANK;
                            snprintf(state->status_msg, sizeof(state->status_msg), "Global visual selection started");
                        } else {
                            editor_global_yank(state);
                            state->visual_selection_mode = VISUAL_MODE_NONE;
                        }
                        break;
                    case 'p': editor_paste(state); break;
                    case 's':
                        if (state->visual_selection_mode == VISUAL_MODE_NONE) {
                            state->selection_start_line = state->current_line;
                            state->selection_start_col = state->current_col;
                            state->visual_selection_mode = VISUAL_MODE_SELECT;
                            snprintf(state->status_msg, sizeof(state->status_msg), "Visual selection started");
                        } else {
                            state->visual_selection_mode = VISUAL_MODE_NONE;
                        }
                        break;
                    case 'y':
                        if (state->visual_selection_mode == VISUAL_MODE_NONE) {
                            state->selection_start_line = state->current_line;
                            state->selection_start_col = state->current_col;
                            state->visual_selection_mode = VISUAL_MODE_YANK;
                            snprintf(state->status_msg, sizeof(state->status_msg), "Visual selection for yank started");
                        } else {
                            editor_yank_selection(state);
                            state->visual_selection_mode = VISUAL_MODE_NONE;
                        }
                        break;
                    case 'm':
                        if (state->visual_selection_mode != VISUAL_MODE_NONE) {
                            editor_yank_to_move_register(state);
                            editor_delete_selection(state);
                            state->is_moving = true;
                            snprintf(state->status_msg, sizeof(state->status_msg), "Text cut. Press 'm' again to paste.");
                        }
                        break;
                    default: // Fallback to normal mode keys
                        // (The original NORMAL mode switch case is now here)
                        switch (ch) {
                            case 'v': state->visual_selection_mode = VISUAL_MODE_NONE; state->mode = NORMAL; break;
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
                                } else {
                                    if (state->current_line > 0) {
                                        state->current_line--;
                                        state->current_col = state->ideal_col;
                                    }
                                }
                                break;
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
                }
                break;
            case NORMAL:
                switch (ch) {
                    case 'm':
                        if (state->is_moving) {
                            editor_paste_from_move_register(state);
                            state->is_moving = false;
                            free(state->move_register);
                            state->move_register = NULL;
                            snprintf(state->status_msg, sizeof(state->status_msg), "Text moved.");
                        }
                        break;
                    case 16: // Ctrl+P
                        editor_global_paste(state);
                        break;
                    case 'v': state->mode = VISUAL; break;
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
                        } else {
                            if (state->current_line > 0) {
                                state->current_line--;
                                state->current_col = state->ideal_col;
                            }
                        }
                        break;
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
        EditorState *estado_janela = gerenciador.janelas[i]->estado;
        if (estado_janela->lsp_enabled) {
            lsp_shutdown(estado_janela);
        }
        free_janela_editor(gerenciador.janelas[i]);
    }
    free(gerenciador.janelas);
       
    stop_and_log_work();
    endwin(); 
    return 0;
}
