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
#include <unistd.h> // For getcwd()
#include <errno.h>      // For the errno variable
#include <locale.h>
#include <sys/select.h>
#include <sys/wait.h> 

const int ansi_to_ncurses_map[16] = {
    COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_YELLOW,
    COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN, COLOR_WHITE,
    COLOR_BLACK,   // Bright Black (usually gray)
    COLOR_RED,     // Bright Red
    COLOR_GREEN,   // Bright Green
    COLOR_YELLOW,  // Bright Yellow
    COLOR_BLUE,    // Bright Blue
    COLOR_MAGENTA, // Bright Magenta
    COLOR_CYAN,    // Bright Cyan
    COLOR_WHITE    // Bright White
};


void inicializar_ncurses() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE);
    set_escdelay(25);
    start_color();
    
    // FIX: Replaces use_default_colors() with a robust solution
    // Tells ncurses to map the terminal's default colors to COLOR_WHITE and COLOR_BLACK
    assume_default_colors(COLOR_WHITE, COLOR_BLACK);

    // Editor color pairs (will now use COLOR_BLACK as a "transparent" background)
    init_pair(1, COLOR_BLACK, COLOR_BLUE);
    init_pair(2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(3, COLOR_YELLOW, COLOR_BLACK);
    init_pair(4, COLOR_GREEN, COLOR_BLACK);
    init_pair(5, COLOR_BLUE, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(8, COLOR_WHITE, COLOR_BLACK); // Default pair: white text, transparent background
    init_pair(9, COLOR_BLACK, COLOR_MAGENTA);
    init_pair(10, COLOR_GREEN, COLOR_BLACK);
    init_pair(11, COLOR_RED, COLOR_BLACK);
    init_pair(12, COLOR_BLACK, COLOR_YELLOW);

    // Color pairs for the terminal (will also use the "transparent" background)
    for (int i = 0; i < 16; i++) {
        init_pair(16 + i, ansi_to_ncurses_map[i], COLOR_BLACK);
    }
    
    bkgd(COLOR_PAIR(8));
}

void process_editor_input(EditorState *state, wint_t ch, bool *should_exit) {
    JanelaEditor* active_jw = ACTIVE_WS->janelas[ACTIVE_WS->janela_ativa_idx];
    WINDOW *active_win = active_jw->win;

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
                return; // Use return to prevent further processing
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
                return;
            case KEY_ENTER: case '\n':
                editor_apply_completion(state);
                return;
            case 27: // ESC
                editor_end_completion(state);
                return;
            default: 
                editor_end_completion(state);
                // Let the rest of the function process the key
                break;
        }
    }
        
    if (ch == 27) { // ESC
        nodelay(active_win, TRUE);
        int next_ch = wgetch(active_win);
        nodelay(active_win, FALSE);

        if (next_ch == ERR) { // Just ESC
             if (state->mode == INSERT || state->mode == VISUAL) {
                state->mode = NORMAL;
            }
             if (state->is_moving) {
                 state->is_moving = false;
                 free(state->move_register);
                 state->move_register = NULL;
                 snprintf(state->status_msg, sizeof(state->status_msg), "Move cancelled.");
             }
        } else { // Alt sequence
            if (next_ch == 'n') ciclar_workspaces(-1);
            else if (next_ch == 'm') ciclar_workspaces(1);
            else if (next_ch == '\n' || next_ch == KEY_ENTER) criar_nova_janela(NULL);
            else if (next_ch == 'x' || next_ch == 'X') fechar_janela_ativa(should_exit);
            else if (next_ch == 'b' || next_ch == 'B') display_recent_files();
            else if (next_ch == 'd' || next_ch == 'D') prompt_and_create_gdb_workspace();
            else if (next_ch == 'z' || next_ch == 'Z') do_undo(state);
            else if (next_ch == 'y' || next_ch == 'Y') do_redo(state);
            else if (next_ch == 'h' || next_ch == 'H') gf2_starter();
            else if (next_ch == 'f' || next_ch == 'w') editor_move_to_next_word(state);
            else if (next_ch == 'b' || next_ch == 'q') editor_move_to_previous_word(state);
            else if (next_ch == 'g' || next_ch == 'G') prompt_for_directory_change(state);
            else if (next_ch == '.' || next_ch == '>') ciclar_layout();
            else if (next_ch >= '1' && next_ch <= '9') mover_janela_para_workspace(next_ch - '1');
            else if (strchr("!@#$%^&*(", next_ch)) {
                const char* symbols = "!@#$%^&*(";
                char* p = strchr(symbols, next_ch);
                if (p) mover_janela_para_posicao(p - symbols);
            }
            else if (next_ch == 'r' || next_ch == 'R') rotacionar_janelas();
            else if (next_ch == 'o' || next_ch == 'O') {
                if (state->mode == VISUAL && state->visual_selection_mode != VISUAL_MODE_NONE) copy_selection_to_clipboard(state);
            }
            else if (next_ch == 'p' || next_ch == 'P') {
                if (state->mode == NORMAL || state->mode == INSERT) paste_from_clipboard(state);
                else if (state->mode == VISUAL && state->visual_selection_mode != VISUAL_MODE_NONE) {
                    editor_delete_selection(state);
                    paste_from_clipboard(state);
                }
            }
        }
        return; // End processing here
    }

    if (ch == KEY_CTRL_W) {
        criar_novo_workspace();
        return;
    }
        switch (state->mode) {
            case VISUAL:
                switch (ch) {
                    case KEY_ENTER:
                    case '\n':
                        editor_handle_enter(state);
                        break;
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
                        switch (ch) {
                            case 'v': state->mode = NORMAL; break;
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
                            case 'o':
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
                            case 'l':
                            case KEY_DOWN: {
                                if (state->current_line < state->num_lines - 1) {
                                    state->current_line++;
                                    state->current_col = state->ideal_col;
                                }
                                break;
                            }
                            case 'k':
                            case KEY_LEFT:
                                if (state->current_col > 0) {
                                    state->current_col--;
                                    while (state->current_col > 0 && (state->lines[state->current_line][state->current_col] & 0xC0) == 0x80) {
                                        state->current_col--;
                                    }
                                }
                                state->ideal_col = state->current_col;
                                break;
                            case 231: // ç
                            case KEY_RIGHT: {
                                char* line = state->lines[state->current_line];
                                if (line && state->current_col < (int)strlen(line)) {
                                    state->current_col++;
                                    while (line[state->current_col] != '\0' && (line[state->current_col] & 0xC0) == 0x80) {
                                        state->current_col++;
                                    }
                                }
                                state->ideal_col = state->current_col;
                                } break;
                            case 'O':
                            case KEY_PPAGE: case KEY_SR: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line > 0) state->current_line--; state->current_col = state->ideal_col; break;
                            case 'L':
                            case KEY_NPAGE: case KEY_SF: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line < state->num_lines - 1) state->current_line++; state->current_col = state->ideal_col; break;
                            case 'K':
                            case KEY_HOME: state->current_col = 0; state->ideal_col = 0; break;
                            case 199: // Ç
                            case KEY_END: { char* line = state->lines[state->current_line]; if(line) state->current_col = strlen(line); state->ideal_col = state->current_col; } break;
                            case KEY_SDC: editor_delete_line(state); break;
                        }
                }
                break;
            case NORMAL:
                switch (ch) {
                    case 'u':
                        state->current_col = 0;
                        state->ideal_col = 0;
                        editor_handle_enter(state);
                        state->current_line--;
                        state->mode = INSERT;
                        break;
                    case 'U':
                        state->current_col = strlen(state->lines[state->current_line]);
                        editor_handle_enter(state);
                        break;
                        state->mode = INSERT;
                    case 'G':
                        state->current_line = state->num_lines - 1;
                        state->current_col = 0;
                        state->ideal_col = 0;
                        break;
                    case 'g':
                        state->current_line = 0;
                        state->current_col = 0;
                        state->ideal_col = 0;
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
                    case 'o':
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
                    case 'l':
                    case KEY_DOWN: {
                        if (state->current_line < state->num_lines - 1) {
                            state->current_line++;
                            state->current_col = state->ideal_col;
                        }
                        break;                        
                    }
                    case 'k':
                    case KEY_LEFT:
                        if (state->current_col > 0) {
                            state->current_col--;
                            while (state->current_col > 0 && (state->lines[state->current_line][state->current_col] & 0xC0) == 0x80) {
                                state->current_col--;
                            }
                        }
                        state->ideal_col = state->current_col;
                        break;
                    case 231: // ç
                    case KEY_RIGHT: {
                        char* line = state->lines[state->current_line];
                        if (line && state->current_col < (int)strlen(line)) {
                            state->current_col++;
                            while (line[state->current_col] != '\0' && (line[state->current_col] & 0xC0) == 0x80) {
                                state->current_col++;
                            }
                        }
                        state->ideal_col = state->current_col;
                        } break;
                    case 'O':
                    case KEY_PPAGE: case KEY_SR: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line > 0) state->current_line--; state->current_col = state->ideal_col; break;
                    case 'L':
                    case KEY_NPAGE: case KEY_SF: for (int i = 0; i < PAGE_JUMP; i++) if (state->current_line < state->num_lines - 1) state->current_line++; state->current_col = state->ideal_col; break;
                    case 'K':
                    case KEY_HOME: state->current_col = 0; state->ideal_col = 0; break;
                    case 199: // Ç
                    case KEY_END: { char* line = state->lines[state->current_line]; if(line) state->current_col = strlen(line); state->ideal_col = state->current_col; } break;
                    case KEY_SDC: editor_delete_line(state); break;
                }
                break;
            case INSERT:
                handle_insert_mode_key(state, ch);
                break;
            case COMMAND:
                handle_command_mode_key(state, ch, should_exit);
                break;
        }
    }    


bool handle_global_shortcut(int ch, bool *should_exit) {
    switch (ch) {
        // --- Workspace Shortcuts ---
        case 'n':
        case 'm':
            ciclar_workspaces(ch == 'm' ? 1 : -1);
            return true; // Shortcut consumed

        // --- Window Shortcuts ---
        case 'x':
        case 'X':
            fechar_janela_ativa(should_exit);
            return true; // Shortcut consumed
            
        case '\n':
        case KEY_ENTER:
            criar_nova_janela(NULL);
            return true; // Shortcut consumed
        
        // Add other global Alt shortcuts HERE

        default:
            return false; // Not a global shortcut, so the active window should handle it
    }
}
int main(int argc, char *argv[]) {
    char exe_path_buf[PATH_MAX];
    if (realpath(argv[0], exe_path_buf)) {
        char* dir = dirname(exe_path_buf);
        if (dir) {
            strncpy(executable_dir, dir, PATH_MAX - 1);
            executable_dir[PATH_MAX - 1] = '\0';
        }
    }

    start_work_timer();
    setlocale(LC_ALL, "");
    inicializar_ncurses();
    
    inicializar_workspaces();

    EditorState *initial_state = ACTIVE_WS->janelas[0]->estado;
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        update_directory_access(initial_state, cwd);
    }

    if (argc > 1) {
        load_file(ACTIVE_WS->janelas[0]->estado, argv[1]);
        EditorState *state = ACTIVE_WS->janelas[0]->estado;

        napms(100);
        lsp_initialize(state);
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
    }

    bool should_exit = false;
    int check_counter = 0;
    while (!should_exit) {
        if (gerenciador_workspaces.num_workspaces == 0) {
            should_exit = true;
            continue;
        }
        
        redesenhar_todas_as_janelas();
        
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        int max_fd = STDIN_FILENO;

        // Add terminal and LSP FDs to the select set
        for (int i = 0; i < gerenciador_workspaces.num_workspaces; i++) {
            GerenciadorJanelas *ws = gerenciador_workspaces.workspaces[i];
            for (int j = 0; j < ws->num_janelas; j++) {
                JanelaEditor *jw = ws->janelas[j];
                if (jw->tipo == TIPOJANELA_TERMINAL && jw->pty_fd != -1) {
                    FD_SET(jw->pty_fd, &readfds);
                    if (jw->pty_fd > max_fd) max_fd = jw->pty_fd;
                } else if (jw->tipo == TIPOJANELA_EDITOR && jw->estado && jw->estado->lsp_client && jw->estado->lsp_client->stdout_fd != -1) {
                    FD_SET(jw->estado->lsp_client->stdout_fd, &readfds);
                    if (jw->estado->lsp_client->stdout_fd > max_fd) max_fd = jw->estado->lsp_client->stdout_fd;
                }
            }
        }

        // Use a timeout to make the loop non-blocking
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000; // 50ms

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0 && errno != EINTR) {
            perror("select error");
            continue;
        }

        // Process keyboard input
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            JanelaEditor *active_jw = ACTIVE_WS->janelas[ACTIVE_WS->janela_ativa_idx];
            if (active_jw->tipo == TIPOJANELA_EDITOR) {
                wint_t ch;
                if (wget_wch(stdscr, &ch) != ERR) {
                     process_editor_input(active_jw->estado, ch, &should_exit);
                }
            } else if (active_jw->tipo == TIPOJANELA_TERMINAL && active_jw->pty_fd != -1) {
                char input_buf[256];
                ssize_t len = read(STDIN_FILENO, input_buf, sizeof(input_buf));
                if (len > 0) {
                    bool atalho_consumido = false;
                    if (len == 2 && input_buf[0] == 27) { // Check for Alt + key
                        if (handle_global_shortcut(input_buf[1], &should_exit)) {
                            atalho_consumido = true;
                        }
                    }
                    if (!atalho_consumido) {
                        write(active_jw->pty_fd, input_buf, len);
                    }
                }
            }
        }

        // Process terminal and LSP output
        for (int i = 0; i < gerenciador_workspaces.num_workspaces; i++) {
            GerenciadorJanelas *ws = gerenciador_workspaces.workspaces[i];
            for (int j = 0; j < ws->num_janelas; j++) {
                JanelaEditor *jw = ws->janelas[j];
                // Process terminal output
                if (jw->tipo == TIPOJANELA_TERMINAL && jw->pty_fd != -1 && FD_ISSET(jw->pty_fd, &readfds)) {
                    char buffer[4096];
                    ssize_t bytes_lidos = read(jw->pty_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_lidos > 0) {
                        buffer[bytes_lidos] = '\0';
                        vterm_render(jw->vterm, buffer, bytes_lidos);
                    } else {
                        // Terminal process finished, convert to a "dead" editor window
                        close(jw->pty_fd);
                        jw->pty_fd = -1;
                        waitpid(jw->pid, NULL, 0);
                        jw->pid = -1;
                        jw->tipo = TIPOJANELA_EDITOR; 
                        jw->estado = calloc(1, sizeof(EditorState));
                        if (jw->estado) {
                            jw->estado->num_lines = 1;
                            jw->estado->lines[0] = strdup("[Processo finalizado. Pressione Alt+X para fechar]");
                            strcpy(jw->estado->filename, "[Terminal Output]");
                        }
                    }
                }
                // Process LSP output
                else if (jw->tipo == TIPOJANELA_EDITOR && jw->estado && jw->estado->lsp_client && jw->estado->lsp_client->stdout_fd != -1 && FD_ISSET(jw->estado->lsp_client->stdout_fd, &readfds)) {
                    char buffer[4096];
                    ssize_t bytes_lidos = read(jw->estado->lsp_client->stdout_fd, buffer, sizeof(buffer) - 1);
                    if (bytes_lidos > 0) {
                        buffer[bytes_lidos] = '\0';
                        lsp_process_received_data(jw->estado, buffer, bytes_lidos);
                    }
                }
            }
        }
        
        // Periodically check for dead processes
        if (check_counter++ % 10 == 0) {
             int status;
             while (waitpid(-1, &status, WNOHANG) > 0);
        }
    }
    
    for (int i = 0; i < gerenciador_workspaces.num_workspaces; i++) {
        free_workspace(gerenciador_workspaces.workspaces[i]);
    }
    free(gerenciador_workspaces.workspaces);
        
    stop_and_log_work();
    endwin(); 
    return 0;
}
