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

// Macro to easily access the active workspace's window manager
#define ACTIVE_WS (gerenciador_workspaces.workspaces[gerenciador_workspaces.workspace_ativo_idx])

// ===================================================================
//  1. Workspace and Window Management
// ===================================================================

// --- Memory Management ---

void free_editor_state(EditorState* state) {
    if (!state) return;
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
    free(state);
}

void free_janela_editor(JanelaEditor* jw) {
    if (!jw) return;
    if (jw->estado) free_editor_state(jw->estado);
    if (jw->win) delwin(jw->win);
    free(jw);
}

void free_workspace(GerenciadorJanelas *ws) {
    if (!ws) return;
    for (int i = 0; i < ws->num_janelas; i++) {
        free_janela_editor(ws->janelas[i]);
    }
    free(ws->janelas);
    free(ws);
}

// --- Initialization ---

void inicializar_workspaces() {
    gerenciador_workspaces.workspaces = NULL;
    gerenciador_workspaces.num_workspaces = 0;
    gerenciador_workspaces.workspace_ativo_idx = -1;
    criar_novo_workspace(); // Create the first initial workspace
}

// --- Workspace Operations ---

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

    // Create an initial window inside the new workspace
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

    // Add window to destination workspace
    dest_ws->num_janelas++;
    dest_ws->janelas = realloc(dest_ws->janelas, sizeof(JanelaEditor*) * dest_ws->num_janelas);
    dest_ws->janelas[dest_ws->num_janelas - 1] = win_to_move;

    // Remove window from source workspace
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

// --- Window (Split) Operations within a Workspace ---

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
    if (state->buffer_modified) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
        return;
    }

    if (ws->num_janelas == 1) {
        fechar_workspace_ativo(should_exit);
        return;
    }

    free_janela_editor(ws->janelas[idx]);
    for (int i = idx; i < ws->num_janelas - 1; i++) {
        ws->janelas[i] = ws->janelas[i+1];
    }
    ws->num_janelas--;
    ws->janelas = realloc(ws->janelas, sizeof(JanelaEditor*) * ws->num_janelas);

    if (ws->janela_ativa_idx >= ws->num_janelas) {
        ws->janela_ativa_idx = ws->num_janelas - 1;
    }

    recalcular_layout_janelas();
}

void fechar_workspace_ativo(bool *should_exit) {
    if (gerenciador_workspaces.num_workspaces == 1) {
        // If it's the last workspace, check the last window for unsaved changes and then exit
        EditorState *last_state = ACTIVE_WS->janelas[0]->estado;
        if (last_state->buffer_modified) {
            snprintf(last_state->status_msg, sizeof(last_state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
            return;
        }
        *should_exit = true;
    }

    int idx_to_close = gerenciador_workspaces.workspace_ativo_idx;
    free_workspace(gerenciador_workspaces.workspaces[idx_to_close]);

    for (int i = idx_to_close; i < gerenciador_workspaces.num_workspaces - 1; i++) {
        gerenciador_workspaces.workspaces[i] = gerenciador_workspaces.workspaces[i+1];
    }
    gerenciador_workspaces.num_workspaces--;
    gerenciador_workspaces.workspaces = realloc(gerenciador_workspaces.workspaces, sizeof(GerenciadorJanelas*) * gerenciador_workspaces.num_workspaces);

    if (gerenciador_workspaces.workspace_ativo_idx >= gerenciador_workspaces.num_workspaces) {
        gerenciador_workspaces.workspace_ativo_idx = gerenciador_workspaces.num_workspaces - 1;
    }
    
    if (gerenciador_workspaces.num_workspaces > 0) {
        recalcular_layout_janelas();
        redesenhar_todas_as_janelas();
    }
}

// --- Layout and Drawing ---

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
    }
}

void redesenhar_todas_as_janelas() {
    erase();
    wnoutrefresh(stdscr);

    if (gerenciador_workspaces.num_workspaces == 0) return;

    GerenciadorJanelas *ws = ACTIVE_WS;
    for (int i = 0; i < ws->num_janelas; i++) {
        JanelaEditor *jw = ws->janelas[i];
        editor_redraw(jw->win, jw->estado);
    }
    
    if (ws->num_janelas > 0) {
        JanelaEditor* active_jw = ws->janelas[ws->janela_ativa_idx];
        EditorState* state = active_jw->estado;
        if (state->lsp_enabled) {
            LspDiagnostic *diag = get_diagnostic_under_cursor(state);
            if (diag) {
                draw_diagnostic_popup(active_jw->win, state, diag->message);
            }
        }
        // state->status_msg[0] = '\0'; // Keep status message until next action
    }
    posicionar_cursor_ativo();
    doupdate();
}

void posicionar_cursor_ativo() {
    if (gerenciador_workspaces.num_workspaces == 0) return;
    GerenciadorJanelas *ws = ACTIVE_WS;
    if (ws->num_janelas == 0) { curs_set(0); return; };

    JanelaEditor* active_jw = ws->janelas[ws->janela_ativa_idx];
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

// --- Navigation ---

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

// display_recent_files remains largely the same, but needs to use ACTIVE_WS
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