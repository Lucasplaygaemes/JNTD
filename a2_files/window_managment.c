#include "window_managment.h"
#include "defs.h"
#include "fileio.h"
#include "others.h"
#include "screen_ui.h"
#include "lsp_client.h"
#include "direct_navigation.h"
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <pty.h>
#include <libgen.h>
#include <signal.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <vterm.h>


#include <sys/select.h> // For select() and fd_set

#define ACTIVE_WS (gerenciador_workspaces.workspaces[gerenciador_workspaces.workspace_ativo_idx])

void desenhar_janela_terminal(JanelaEditor *jw);
void criar_novo_workspace_vazio();

void atualizar_tamanho_pty(JanelaEditor *jw) {
    if (jw->tipo != TIPOJANELA_TERMINAL || jw->pty_fd == -1) return;

    int border_offset = ACTIVE_WS->num_janelas > 1 ? 1 : 0;
    struct winsize ws;
    ws.ws_row = jw->altura - (2 * border_offset);
    ws.ws_col = jw->largura - (2 * border_offset);
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;
    
    ioctl(jw->pty_fd, TIOCSWINSZ, &ws);
}



void criar_janela_terminal_generica(char *const argv[]) {
    if (!argv || !argv[0]) return;

    GerenciadorJanelas *ws = ACTIVE_WS;
    ws->num_janelas++;
    ws->janelas = realloc(ws->janelas, sizeof(JanelaEditor*) * ws->num_janelas);
    if (!ws->janelas) { perror("realloc failed"); ws->num_janelas--; exit(1); }

    JanelaEditor *jw = calloc(1, sizeof(JanelaEditor));
    if (!jw) { perror("calloc failed"); ws->num_janelas--; exit(1); }
    ws->janelas[ws->num_janelas - 1] = jw;
    ws->janela_ativa_idx = ws->num_janelas - 1;

    int master_fd;
    pid_t pid = forkpty(&master_fd, NULL, NULL, NULL);

    if (pid < 0) {
        perror("forkpty failed");
        ws->num_janelas--; free(jw); return;
    }
    if (pid == 0) {
        execvp(argv[0], argv);
        exit(127);
    }
    
    // Recalculate the layout BEFORE creating the vterm to get the correct size
    recalcular_layout_janelas();
    
    jw->tipo = TIPOJANELA_TERMINAL;
    jw->pid = pid;
    jw->pty_fd = master_fd;
    fcntl(master_fd, F_SETFL, O_NONBLOCK);

    // CHANGE: vterm creation logic adapted for the new API
    int rows, cols;
    getmaxyx(jw->win, rows, cols);
    int border_offset = ws->num_janelas > 1 ? 1 : 0;

    jw->vterm = vterm_create(cols - 2 * border_offset, rows - 2 * border_offset, VTERM_FLAG_XTERM_256);
    vterm_wnd_set(jw->vterm, jw->win); // Associates the ncurses WINDOW
    vterm_set_userptr(jw->vterm, jw);  // Associates our data with the instance
}


void handle_gdb_session(int pty_fd, pid_t child_pid);

void free_editor_state(EditorState* state) {
    if (!state) return;

    // Properly shut down the LSP client before freeing the state
    if (state->lsp_client) {
        lsp_shutdown(state);
    }

    if (state->filename[0] != '[') {
        save_last_line(state->filename, state->current_line);
    }
    if (state->completion_mode != COMPLETION_NONE) editor_end_completion(state);
    for(int j=0; j < state->history_count; j++) free(state->command_history[j]);
    for (int j = 0; j < state->undo_count; j++) free_snapshot(state->undo_stack[j]);
    for (int j = 0; j < state->redo_count; j++) free_snapshot(state->redo_stack[j]);
    if (state->syntax_rules) {
        for (int j = 0; j < state->num_syntax_rules; j++) free(state->syntax_rules[j].word);
        free(state->syntax_rules);
    }
    if (state->recent_dirs) {
        for (int j = 0; j < state->num_recent_dirs; j++) free(state->recent_dirs[j]->path);
        free(state->recent_dirs);
    }
    if (state->recent_files) {
        for (int j = 0; j < state->num_recent_files; j++) free(state->recent_files[j]->path);
        free(state->recent_files);
    }
    if (state->unmatched_brackets) free(state->unmatched_brackets);
    if (state->yank_register) free(state->yank_register);
    if (state->move_register) free(state->move_register);
    for (int j = 0; j < state->num_lines; j++) {
        if (state->lines[j]) free(state->lines[j]);
    }
    for (int j = 0; j < 26; j++) {
        if(state->macro_registers[j]) free(state->macro_registers[j]);
    }
    free(state);
}

void free_janela_editor(JanelaEditor* jw) {
    if (!jw) return;

    if (jw->tipo == TIPOJANELA_EDITOR && jw->estado) {
        free_editor_state(jw->estado);
    } else if (jw->tipo == TIPOJANELA_TERMINAL) {
        if (jw->pid > 0) { kill(jw->pid, SIGKILL); waitpid(jw->pid, NULL, 0); }
        if (jw->pty_fd != -1) close(jw->pty_fd);
        if (jw->vterm) vterm_destroy(jw->vterm); // Use vterm_destroy
    }

    if (jw->win) delwin(jw->win);
    free(jw);
}


void inicializar_workspaces() {
    gerenciador_workspaces.workspaces = NULL;
    gerenciador_workspaces.num_workspaces = 0;
    gerenciador_workspaces.workspace_ativo_idx = -1;
    criar_novo_workspace();
}

void criar_novo_workspace() {
    gerenciador_workspaces.num_workspaces++;
    gerenciador_workspaces.workspaces = realloc(gerenciador_workspaces.workspaces, sizeof(GerenciadorJanelas*) * gerenciador_workspaces.num_workspaces);

    GerenciadorJanelas *novo_ws = calloc(1, sizeof(GerenciadorJanelas));
    novo_ws->janelas = NULL;
    novo_ws->num_janelas = 0;
    novo_ws->janela_ativa_idx = -1;
    novo_ws->current_layout = LAYOUT_VERTICAL_SPLIT;

    gerenciador_workspaces.workspaces[gerenciador_workspaces.num_workspaces - 1] = novo_ws;
    gerenciador_workspaces.workspace_ativo_idx = gerenciador_workspaces.num_workspaces - 1;

    criar_nova_janela(NULL);
}

void ciclar_workspaces(int direcao) {
    if (gerenciador_workspaces.num_workspaces <= 1) return;

    gerenciador_workspaces.workspace_ativo_idx += direcao;

    if (gerenciador_workspaces.workspace_ativo_idx >= gerenciador_workspaces.num_workspaces) {
        gerenciador_workspaces.workspace_ativo_idx = 0;
    }
    if (gerenciador_workspaces.workspace_ativo_idx < 0) {
        gerenciador_workspaces.workspace_ativo_idx = gerenciador_workspaces.num_workspaces - 1;
    }

    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void mover_janela_para_workspace(int target_idx) {
    if (target_idx < 0 || target_idx >= gerenciador_workspaces.num_workspaces || target_idx == gerenciador_workspaces.workspace_ativo_idx) {
        return;
    }
    if (ACTIVE_WS->num_janelas <= 1) {
        snprintf(ACTIVE_WS->janelas[0]->estado->status_msg, STATUS_MSG_LEN, "Cannot move the last window of a workspace.");
        return;
    }

    GerenciadorJanelas *source_ws = ACTIVE_WS;
    GerenciadorJanelas *dest_ws = gerenciador_workspaces.workspaces[target_idx];
    int active_win_idx = source_ws->janela_ativa_idx;
    JanelaEditor *win_to_move = source_ws->janelas[active_win_idx];

    dest_ws->num_janelas++;
    dest_ws->janelas = realloc(dest_ws->janelas, sizeof(JanelaEditor*) * dest_ws->num_janelas);
    dest_ws->janelas[dest_ws->num_janelas - 1] = win_to_move;

    for (int i = active_win_idx; i < source_ws->num_janelas - 1; i++) {
        source_ws->janelas[i] = source_ws->janelas[i+1];
    }
    source_ws->num_janelas--;
    if (source_ws->num_janelas > 0) {
        source_ws->janelas = realloc(source_ws->janelas, sizeof(JanelaEditor*) * source_ws->num_janelas);
    } else {
        free(source_ws->janelas);
        source_ws->janelas = NULL;
    }
    
    if (source_ws->janela_ativa_idx >= source_ws->num_janelas) {
        source_ws->janela_ativa_idx = source_ws->num_janelas - 1;
    }

    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void criar_nova_janela(const char *filename) {
    GerenciadorJanelas *ws = ACTIVE_WS;
    ws->num_janelas++;
    ws->janelas = realloc(ws->janelas, sizeof(JanelaEditor*) * ws->num_janelas);

    JanelaEditor *nova_janela = calloc(1, sizeof(JanelaEditor));
    nova_janela->estado = calloc(1, sizeof(EditorState));
    EditorState *state = nova_janela->estado;

    strcpy(state->filename, "[No Name]");
    state->mode = NORMAL;
    state->completion_mode = COMPLETION_NONE;
    state->buffer_modified = false;
    state->auto_indent_on_newline = true;
    state->last_auto_save_time = time(NULL);
    state->word_wrap_enabled = true;
    load_directory_history(state);
    load_file_history(state);

    state->is_recording_macro = false;
    state->last_played_macro_register = 0;
    state->single_command_mode = false;
    for (int i = 0; i < 26; i++) {
        state->macro_registers[i] = NULL;
    }

    ws->janelas[ws->num_janelas - 1] = nova_janela;
    ws->janela_ativa_idx = ws->num_janelas - 1;

    recalcular_layout_janelas();

    if (filename) {
        load_file(state, filename);
    } else {
        load_syntax_file(state, "c.syntax");
        state->lines[0] = calloc(1, 1);
        state->num_lines = 1;
    }
    push_undo(state);
}

void fechar_janela_ativa(bool *should_exit) {
    GerenciadorJanelas *ws = ACTIVE_WS;
    if (ws->num_janelas == 0) return;

    int idx = ws->janela_ativa_idx;
    EditorState *state = ws->janelas[idx]->estado;
    if (state && state->buffer_modified) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
        return;
    }

    if (ws->num_janelas == 1) {
        fechar_workspace_ativo(should_exit);
        return;
    }

    JanelaEditor *jw_to_free = ws->janelas[idx];

    // Shift the array to remove the pointer
    for (int i = idx; i < ws->num_janelas - 1; i++) {
        ws->janelas[i] = ws->janelas[i+1];
    }
    ws->num_janelas--;
    
    // Safely reallocate the array, checking for errors.
    JanelaEditor **new_janelas = realloc(ws->janelas, sizeof(JanelaEditor*) * ws->num_janelas);
    if (!new_janelas) {
        // If realloc fails, we are in a bad state. Exit gracefully.
        perror("realloc failed when closing window");
        exit(EXIT_FAILURE);
    }
    ws->janelas = new_janelas;

    // Update the active index
    if (ws->janela_ativa_idx >= ws->num_janelas) {
        ws->janela_ativa_idx = ws->num_janelas - 1;
    }

    // Now it is safe to free the memory
    free_janela_editor(jw_to_free);

    recalcular_layout_janelas();
}

void fechar_workspace_ativo(bool *should_exit) {
    if (gerenciador_workspaces.num_workspaces == 0) return;

    // If it's the last workspace, just signal the main loop to exit.
    // The cleanup at the end of main() will handle freeing the last workspace.
    if (gerenciador_workspaces.num_workspaces == 1) {
        EditorState *last_state = ACTIVE_WS->janelas[0]->estado;
        if (last_state && last_state->buffer_modified) {
            snprintf(last_state->status_msg, sizeof(last_state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
            return;
        }
        *should_exit = true;
        return;
    }

    int idx_to_close = gerenciador_workspaces.workspace_ativo_idx;
    GerenciadorJanelas *ws_to_free = gerenciador_workspaces.workspaces[idx_to_close];

    // Shift the array to remove the pointer
    for (int i = idx_to_close; i < gerenciador_workspaces.num_workspaces - 1; i++) {
        gerenciador_workspaces.workspaces[i] = gerenciador_workspaces.workspaces[i+1];
    }
    gerenciador_workspaces.num_workspaces--;
    
    // Safely reallocate the array, checking for errors.
    GerenciadorJanelas **new_workspaces = realloc(gerenciador_workspaces.workspaces, sizeof(GerenciadorJanelas*) * gerenciador_workspaces.num_workspaces);
    if (!new_workspaces) {
        perror("realloc failed when closing workspace");
        exit(EXIT_FAILURE);
    }
    gerenciador_workspaces.workspaces = new_workspaces;

    // Update active index
    if (gerenciador_workspaces.workspace_ativo_idx >= gerenciador_workspaces.num_workspaces) {
        gerenciador_workspaces.workspace_ativo_idx = gerenciador_workspaces.num_workspaces - 1;
    }
    
    // Now it's safe to free the memory of the closed workspace
    free_workspace(ws_to_free);
    
    // Redraw the UI with the remaining workspaces
    if (gerenciador_workspaces.num_workspaces > 0) {
        recalcular_layout_janelas();
        redesenhar_todas_as_janelas();
    }
}

void recalcular_layout_janelas() {
    GerenciadorJanelas *ws = ACTIVE_WS;
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    if (ws->num_janelas == 0) return;

    if ((ws->num_janelas != 2 && ws->current_layout == LAYOUT_HORIZONTAL_SPLIT) ||
        (ws->num_janelas != 3 && ws->current_layout == LAYOUT_MAIN_AND_STACK) ||
        (ws->num_janelas != 4 && ws->current_layout == LAYOUT_GRID)) {
        ws->current_layout = LAYOUT_VERTICAL_SPLIT;
    }

    switch (ws->current_layout) {
        case LAYOUT_HORIZONTAL_SPLIT:
            if (ws->num_janelas == 2) {
                int altura_janela = screen_rows / 2;
                for (int i = 0; i < 2; i++) {
                    ws->janelas[i]->y = i * altura_janela;
                    ws->janelas[i]->x = 0;
                    ws->janelas[i]->largura = screen_cols;
                    ws->janelas[i]->altura = (i == 1) ? (screen_rows - altura_janela) : altura_janela;
                }
            }
            break;
        case LAYOUT_MAIN_AND_STACK:
            if (ws->num_janelas == 3) {
                int main_width = screen_cols / 2;
                int stack_width = screen_cols - main_width;
                int stack_height = screen_rows / 2;
                ws->janelas[0]->y = 0;
                ws->janelas[0]->x = 0;
                ws->janelas[0]->largura = main_width;
                ws->janelas[0]->altura = screen_rows;
                ws->janelas[1]->y = 0;
                ws->janelas[1]->x = main_width;
                ws->janelas[1]->largura = stack_width;
                ws->janelas[1]->altura = stack_height;
                ws->janelas[2]->y = stack_height;
                ws->janelas[2]->x = main_width;
                ws->janelas[2]->largura = stack_width;
                ws->janelas[2]->altura = screen_rows - stack_height;
            }
            break;
        case LAYOUT_GRID:
            if (ws->num_janelas == 4) {
                int win_w = screen_cols / 2;
                int win_h = screen_rows / 2;
                ws->janelas[0]->y = 0;     ws->janelas[0]->x = 0;
                ws->janelas[1]->y = 0;     ws->janelas[1]->x = win_w;
                ws->janelas[2]->y = win_h; ws->janelas[2]->x = 0;
                ws->janelas[3]->y = win_h; ws->janelas[3]->x = win_w;
                for(int i=0; i<4; i++) {
                    ws->janelas[i]->altura = (i >= 2) ? screen_rows - win_h : win_h;
                    ws->janelas[i]->largura = (i % 2 != 0) ? screen_cols - win_w : win_w;
                }
            }
            break;
        case LAYOUT_VERTICAL_SPLIT:
        default: {
            int largura_janela = screen_cols / ws->num_janelas;
            for (int i = 0; i < ws->num_janelas; i++) {
                ws->janelas[i]->y = 0;
                ws->janelas[i]->x = i * largura_janela;
                ws->janelas[i]->altura = screen_rows;
                ws->janelas[i]->largura = (i == ws->num_janelas - 1) ? (screen_cols - ws->janelas[i]->x) : largura_janela;
            }
            break;
        }
    }


    for (int i = 0; i < ws->num_janelas; i++) {
        JanelaEditor *jw = ws->janelas[i];
        if (jw->win) {
            delwin(jw->win);
        }
        jw->win = newwin(jw->altura, jw->largura, jw->y, jw->x);
        keypad(jw->win, TRUE);
        scrollok(jw->win, FALSE);
        
        // If it's a terminal, we need to resize it
        if (jw->tipo == TIPOJANELA_TERMINAL && jw->vterm) {
            int border_offset = ws->num_janelas > 1 ? 1 : 0;
            int content_h = jw->altura - (2 * border_offset);
            int content_w = jw->largura - (2 * border_offset);
            
            // FIX: Use vterm_resize, which is the correct function from this library
            vterm_resize(jw->vterm, content_w > 0 ? content_w : 1, content_h > 0 ? content_h : 1);
            atualizar_tamanho_pty(jw); // This function remains important
        }
    }
}


void executar_comando_no_terminal(const char *comando_str) {
    // If no command is specified, open a default shell
    if (strlen(comando_str) == 0) {
        char *const cmd[] = {"/bin/bash", NULL};
        criar_janela_terminal_generica(cmd);
        return;
    }

    // Create a copy of the string, as strtok modifies it
    char *str_copia = strdup(comando_str);
    if (!str_copia) return;

    // Array to store the arguments (e.g., "btop", "--utf-force")
    char **argv = NULL;
    int argc = 0;
    
    // Use strtok to split the string into words (tokens) separated by spaces
    char *token = strtok(str_copia, " ");
    while (token != NULL) {
        argc++;
        argv = realloc(argv, sizeof(char*) * argc);
        argv[argc - 1] = token;
        token = strtok(NULL, " ");
    }

    // Add NULL at the end, which is required for the execvp function
    argc++;
    argv = realloc(argv, sizeof(char*) * argc);
    argv[argc - 1] = NULL;

    // Call our magic function that is already prepared!
    if (argv) {
        criar_janela_terminal_generica(argv);
    }

    // Free the memory we allocated
    free(str_copia);
    free(argv);
}

void executar_comando_em_novo_workspace(const char *comando_str) {
    criar_novo_workspace_vazio();

    if (strlen(comando_str) == 0) {
        char *const cmd[] = {"/bin/bash", NULL};
        criar_janela_terminal_generica(cmd);
        return;
    }

    char *str_copia = strdup(comando_str);
    if (!str_copia) return;

    char **argv = NULL;
    int argc = 0;
    
    char *token = strtok(str_copia, " ");
    while (token != NULL) {
        argc++;
        argv = realloc(argv, sizeof(char*) * argc);
        argv[argc - 1] = token;
        token = strtok(NULL, " ");
    }

    argc++;
    argv = realloc(argv, sizeof(char*) * argc);
    argv[argc - 1] = NULL;

    if (argv) {
        criar_janela_terminal_generica(argv);
    }

    free(str_copia);
    free(argv);
}

void free_workspace(GerenciadorJanelas *ws) {
    if (!ws) return;
    for (int i = 0; i < ws->num_janelas; i++) {
        free_janela_editor(ws->janelas[i]);
    }
    free(ws->janelas);
    free(ws);
}

void redesenhar_todas_as_janelas() {
    // Clear the main virtual screen
    erase();
    wnoutrefresh(stdscr);

    if (gerenciador_workspaces.num_workspaces == 0) return;

    GerenciadorJanelas *ws = ACTIVE_WS;    

    // 1. Draw all main windows first
    for (int i = 0; i < ws->num_janelas; i++) {
        JanelaEditor *jw = ws->janelas[i];
        if (jw) {
            // Draw the window border
            if (ws->num_janelas > 1) {
                wattron(jw->win, (i == ws->janela_ativa_idx) ? (COLOR_PAIR(3)|A_BOLD) : 0);
                box(jw->win, 0, 0);
                wattroff(jw->win, (i == ws->janela_ativa_idx) ? (COLOR_PAIR(3)|A_BOLD) : 0);
            }
            
            // Prepare the window content to be drawn
            if (jw->tipo == TIPOJANELA_EDITOR && jw->estado) {
                editor_redraw(jw->win, jw->estado);
            } else if (jw->tipo == TIPOJANELA_TERMINAL && jw->vterm) {
                // Tell libvterm to redraw its content in the associated WINDOW.
                vterm_wnd_update(jw->vterm, -1, 0, VTERM_WND_RENDER_ALL);
            }
            // Add the window to the redraw "queue"
            wnoutrefresh(jw->win);
        }
    }

    // 2. Now, draw the diagnostic popup on top of the active window
    if (ws->num_janelas > 0) {
        JanelaEditor *active_jw = ws->janelas[ws->janela_ativa_idx];
        if (active_jw->tipo == TIPOJANELA_EDITOR && active_jw->estado) {
            EditorState *state = active_jw->estado;
            if (state->lsp_enabled) {
                LspDiagnostic *diag = get_diagnostic_under_cursor(state);
                if (diag) {
                    draw_diagnostic_popup(active_jw->win, state, diag->message);
                }
            }
        }
    }

    // 3. Position the cursor and update the physical screen
    posicionar_cursor_ativo();
    doupdate();
}

void posicionar_cursor_ativo() {
    if (gerenciador_workspaces.num_workspaces == 0 || ACTIVE_WS->num_janelas == 0) {
        curs_set(0);
        return;
    }

    GerenciadorJanelas *ws = ACTIVE_WS;
    if (ws->num_janelas == 0) { curs_set(0); return; };

    JanelaEditor* active_jw = ws->janelas[ACTIVE_WS->janela_ativa_idx];
    
    // If the window is a terminal, libvterm handles the cursor. We do nothing.
    if (active_jw->tipo == TIPOJANELA_TERMINAL) {
        // libvterm has already positioned the cursor during vterm_render or vterm_wnd_update.
        // We just ensure it is visible if the process is active.
        curs_set(active_jw->pid != -1 ? 1 : 0);
    } 
    // If the window is an editor, we handle the cursor manually.
    else if (active_jw->tipo == TIPOJANELA_EDITOR) {
        EditorState* state = active_jw->estado;
        if (!state) { curs_set(0); return; }
        
        WINDOW* win = active_jw->win;
        if (state->completion_mode != COMPLETION_NONE) {
            editor_draw_completion_win(win, state); // Hide the main cursor
        } else {
            curs_set(1); // Turn on the cursor
            if (state->mode == COMMAND) {
                int rows, cols;
                getmaxyx(win, rows, cols);
                (void)cols;
                wmove(win, rows - 1, state->command_pos + 2);
            } else {
                int visual_y, visual_x;
                get_visual_pos(win, state, &visual_y, &visual_x);
                int border_offset = ws->num_janelas > 1 ? 1 : 0;
                int screen_y = visual_y - state->top_line + border_offset;
                int screen_x = visual_x - state->left_col + border_offset;
                int max_y, max_x;
                getmaxyx(win, max_y, max_x);
                if (screen_y >= max_y) screen_y = max_y - 1;
                if (screen_x >= max_x) screen_x = max_x - 1;
                if (screen_y < border_offset) screen_y = border_offset;
                if (screen_x < border_offset) screen_x = border_offset;
                wmove(win, screen_y, screen_x);
            }
        }
    }
    wnoutrefresh(active_jw->win);
}

void proxima_janela() {
    GerenciadorJanelas *ws = ACTIVE_WS;
    if (ws->num_janelas > 1) {
        ws->janela_ativa_idx = (ws->janela_ativa_idx + 1) % ws->num_janelas;
    }
}

void janela_anterior() {
    GerenciadorJanelas *ws = ACTIVE_WS;
    if (ws->num_janelas > 1) {
        ws->janela_ativa_idx = (ws->janela_ativa_idx - 1 + ws->num_janelas) % ws->num_janelas;
    }
}

void ciclar_layout() {
    GerenciadorJanelas *ws = ACTIVE_WS;
    if (ws->num_janelas <= 1) return;

    switch (ws->num_janelas) {
        case 2:
            if (ws->current_layout == LAYOUT_VERTICAL_SPLIT) {
                ws->current_layout = LAYOUT_HORIZONTAL_SPLIT;
            } else {
                ws->current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        case 3:
            if (ws->current_layout == LAYOUT_VERTICAL_SPLIT) {
                ws->current_layout = LAYOUT_MAIN_AND_STACK;
            } else {
                ws->current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        case 4:
            if (ws->current_layout == LAYOUT_VERTICAL_SPLIT) {
                ws->current_layout = LAYOUT_GRID;
            } else {
                ws->current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        default:
            ws->current_layout = LAYOUT_VERTICAL_SPLIT;
            break;
    }

    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void rotacionar_janelas() {
    GerenciadorJanelas *ws = ACTIVE_WS;
    if (ws->num_janelas <= 1) return;
    JanelaEditor *ultima_janela = ws->janelas[ws->num_janelas - 1];
    for (int i = ws->num_janelas - 1; i > 0; i--) {
        ws->janelas[i] = ws->janelas[i - 1];
    }
    ws->janelas[0] = ultima_janela;
    ws->janela_ativa_idx = (ws->janela_ativa_idx + 1) % ws->num_janelas;
    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void mover_janela_para_posicao(int target_idx) {
    GerenciadorJanelas *ws = ACTIVE_WS;
    int active_idx = ws->janela_ativa_idx;
    if (ws->num_janelas <= 1 || target_idx < 0 || target_idx >= ws->num_janelas || target_idx == active_idx) return;
    JanelaEditor *janela_ativa_ptr = ws->janelas[active_idx];
    ws->janelas[active_idx] = ws->janelas[target_idx];
    ws->janelas[target_idx] = janela_ativa_ptr;
    ws->janela_ativa_idx = target_idx;
    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

typedef struct {
    char* path;
    bool is_recent;
} SearchResult;

void display_recent_files() {
    EditorState *active_state = ACTIVE_WS->janelas[ACTIVE_WS->janela_ativa_idx]->estado;
    
    WINDOW *switcher_win;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int win_h = min(20, rows - 4);
    int win_w = cols / 2;
    if (win_w < 60) win_w = 60;
    int win_y = (rows - win_h) / 2;
    int win_x = (cols - win_w) / 2;

    switcher_win = newwin(win_h, win_w, win_y, win_x);
    keypad(switcher_win, TRUE);
    wbkgd(switcher_win, COLOR_PAIR(8));

    int current_selection = 0;
    int top_of_list = 0;
    char search_term[100] = {0};
    int search_pos = 0;
    bool search_mode = false;

    SearchResult *results = NULL;
    int num_results = 0;

    while (1) {
        for(int i = 0; i < num_results; i++) free(results[i].path);
        free(results);
        results = NULL;
        num_results = 0;

        if (search_term[0] != '\0') {
            results = malloc(sizeof(SearchResult) * (active_state->num_recent_files + 1024));
            
            if (results) {
                for (int i = 0; i < active_state->num_recent_files; i++) {
                    if (strstr(active_state->recent_files[i]->path, search_term)) {
                        results[num_results].path = strdup(active_state->recent_files[i]->path);
                        results[num_results].is_recent = true;
                        num_results++;
                    }
                }
            }

            DIR *d;
            struct dirent *dir;
            d = opendir(".");
            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    struct stat st;
                        if (stat(dir->d_name, &st) == 0 && S_ISREG(st.st_mode) && strstr(dir->d_name, search_term)) {
                        char full_path[PATH_MAX];
                        realpath(dir->d_name, full_path);

                        bool already_in_list = false;
                        for (int i = 0; i < num_results; i++) {
                            if (strcmp(results[i].path, full_path) == 0) {
                                already_in_list = true;
                                break;
                            }
                        }

                        if (!already_in_list) {
                            results[num_results].path = strdup(full_path);
                            results[num_results].is_recent = false;
                            num_results++;
                        }
                    }
                }
                closedir(d);
            }
        }

        int list_size = (search_term[0] != '\0') ? num_results : active_state->num_recent_files;
        
        if (current_selection >= list_size) {
            current_selection = list_size > 0 ? list_size - 1 : 0;
        }
        if (top_of_list > current_selection) top_of_list = current_selection;
        if (win_h > 3 && top_of_list < current_selection - (win_h - 3)) {
            top_of_list = current_selection - (win_h - 3);
        }

        werase(switcher_win);
        box(switcher_win, 0, 0);
        mvwprintw(switcher_win, 0, (win_w - 14) / 2, " Open File ");

        for (int i = 0; i < win_h - 2; i++) {
            int item_idx = top_of_list + i;
            if (item_idx >= list_size) break;

            if (item_idx == current_selection) wattron(switcher_win, A_REVERSE);

            char *path_to_show;
            bool is_recent;

            if (search_term[0] != '\0') {
                path_to_show = results[item_idx].path;
                is_recent = results[item_idx].is_recent;
            } else {
                path_to_show = active_state->recent_files[item_idx]->path;
                is_recent = true;
            }
            
            if (!is_recent) wattron(switcher_win, COLOR_PAIR(6));

            char display_name[win_w - 4];
            const char *home_dir = getenv("HOME");
            if (home_dir && strstr(path_to_show, home_dir) == path_to_show) {
                snprintf(display_name, sizeof(display_name), "~%s", path_to_show + strlen(home_dir));
            } else {
                strncpy(display_name, path_to_show, sizeof(display_name) - 1);
            }
            display_name[sizeof(display_name)-1] = '\0';

            mvwprintw(switcher_win, i + 1, 2, "%.*s", win_w - 3, display_name);

            if (!is_recent) wattroff(switcher_win, COLOR_PAIR(6));
            if (item_idx == current_selection) wattroff(switcher_win, A_REVERSE);
        }
        
        mvwprintw(switcher_win, win_h - 1, 1, "/%s", search_term);
        if (search_mode) wmove(switcher_win, win_h - 1, search_pos + 2);

        wrefresh(switcher_win);

        int ch = wgetch(switcher_win);

        switch(ch) {
            case '/':
                if (!search_mode) {
                    search_mode = true;
                    curs_set(1);
                }
                break;
            case KEY_RESIZE:
                getmaxyx(stdscr, rows, cols);
                win_h = min(20, rows - 4);
                win_w = cols / 2;
                if (win_w < 60) win_w = 60;
                win_y = (rows - win_h) / 2;
                win_x = (cols - win_w) / 2;

                wresize(switcher_win, win_h, win_w);
                mvwin(switcher_win, win_y, win_x);

                touchwin(stdscr);
                redesenhar_todas_as_janelas();
                break;
            case KEY_UP: case 'k':
                if (current_selection > 0) current_selection--;
                break;
            case KEY_DOWN: case 'j':
                if (current_selection < list_size - 1) current_selection++;
                break;
            case KEY_ENTER: case '\n':
                {
                    if (list_size == 0 && search_term[0] == '\0') goto end_switcher;

                    if (search_mode) {
                        search_mode = false;
                        curs_set(0);
                        break;
                    }
                    
                    char* selected_file = NULL;
                    if (list_size > 0) {
                        if (search_term[0] != '\0') {
                            selected_file = results[current_selection].path;
                        } else {
                            selected_file = active_state->recent_files[current_selection]->path;
                        }
                    }

                    if (selected_file) {
                        if (active_state->buffer_modified) {
                            delwin(switcher_win);
                            touchwin(stdscr);
                            redesenhar_todas_as_janelas();

                            if (!confirm_action("Unsaved changes. Open file anyway?")) {
                                goto end_switcher;
                            }
                        }
                        
                        load_file(active_state, selected_file);
                        const char * syntax_file = get_syntax_file_from_extension(selected_file);
                        load_syntax_file(active_state, syntax_file);
                    }
                    goto end_switcher;
                }
            case 27: case 'q': // ESC or q
                if (search_mode) {
                    search_mode = false;
                    search_term[0] = '\0';
                    search_pos = 0;
                    curs_set(0);
                    current_selection = 0;
                    top_of_list = 0;
                } else {
                    goto end_switcher;
                }
                break;
            case KEY_BACKSPACE: case 127:
                if (search_mode && search_pos > 0) {
                    search_term[--search_pos] = '\0';
                    current_selection = 0;
                    top_of_list = 0;
                }
                break;
            default:
                if (search_mode && isprint(ch)) {
                    if (search_pos < (int)sizeof(search_term) - 1) {
                        search_term[search_pos++] = ch;
                        search_term[search_pos] = '\0';
                        current_selection = 0;
                        top_of_list = 0;
                    }
                }
                break;
        }
    }

end_switcher:
    for(int i = 0; i < num_results; i++) free(results[i].path);
    free(results);
    delwin(switcher_win);
    touchwin(stdscr);
    redesenhar_todas_as_janelas();
}

void criar_novo_workspace_vazio() {
    gerenciador_workspaces.num_workspaces++;
    gerenciador_workspaces.workspaces = realloc(gerenciador_workspaces.workspaces, sizeof(GerenciadorJanelas*) * gerenciador_workspaces.num_workspaces);

    GerenciadorJanelas *novo_ws = calloc(1, sizeof(GerenciadorJanelas));
    novo_ws->janelas = NULL;
    novo_ws->num_janelas = 0;
    novo_ws->janela_ativa_idx = -1;
    novo_ws->current_layout = LAYOUT_VERTICAL_SPLIT;

    gerenciador_workspaces.workspaces[gerenciador_workspaces.num_workspaces - 1] = novo_ws;
    gerenciador_workspaces.workspace_ativo_idx = gerenciador_workspaces.num_workspaces - 1;
}

void prompt_and_create_gdb_workspace() {
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    // (The code to create the prompt window and get the path_buffer remains the same)
    int win_h = 5;
    int win_w = screen_cols - 20;
    if (win_w < 50) win_w = 50;
    int win_y = (screen_rows - win_h) / 2;
    int win_x = (screen_cols - win_w) / 2;
    WINDOW *input_win = newwin(win_h, win_w, win_y, win_x);
    keypad(input_win, TRUE);
    wbkgd(input_win, COLOR_PAIR(9));
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "Path to executable to debug with GDB:");
    wrefresh(input_win);

    char path_buffer[1024] = {0};
    curs_set(1);
    echo();
    wmove(input_win, 2, 2);
    wgetnstr(input_win, path_buffer, sizeof(path_buffer) - 1);
    noecho();
    curs_set(0);
    delwin(input_win);
    touchwin(stdscr);
    redesenhar_todas_as_janelas();

    if (strlen(path_buffer) > 0) {
        // 1. Create a new empty workspace
        criar_novo_workspace_vazio();
        
        // 2. Add a single terminal window to this new workspace
        char *const cmd[] = {"gdb", "-tui", path_buffer, NULL};
        criar_janela_terminal_generica(cmd);
    }
}

void handle_gdb_session(int pty_fd, pid_t child_pid) {
    // Set terminal to raw mode
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

    // I/O loop
    while (1) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO, &fds);
        FD_SET(pty_fd, &fds);

        select(pty_fd + 1, &fds, NULL, NULL, NULL);

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) > 0) {
                write(pty_fd, &c, 1);
            }
        }

        if (FD_ISSET(pty_fd, &fds)) {
            char buf[1024];
            ssize_t n = read(pty_fd, buf, sizeof(buf));
            if (n > 0) {
                write(STDOUT_FILENO, buf, n);
            } else {
                break; // GDB exited or error
            }
        }

        int status;
        if (waitpid(child_pid, &status, WNOHANG) > 0) {
            break; // Child has exited
        }
    }

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void gf2_starter() {
    // This function now opens 'gf2' in a new terminal window,
    // instead of freezing the editor.
    // You can add a prompt for the user to type the filename,
    // or pass a fixed argument.
    char *const cmd[] = {"gf2", NULL};
    criar_janela_terminal_generica(cmd);
}


