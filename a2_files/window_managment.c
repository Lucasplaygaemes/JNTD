#include "window_managment.h" // Include its own header
#include "defs.h" // For EditorState, etc.
#include "fileio.h" // For save_last_line, load_file, load_syntax_file
#include "others.h" // For editor_end_completion, free_snapshot, push_undo
#include "screen_ui.h" // For editor_redraw, posicionar_cursor_ativo, get_visual_pos, editor_draw_completion_win
#include "lsp_client.h" // For lsp_shutdown
#include "direct_navigation.h"
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

// ===================================================================
//  1. Window Management
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

    for (int j = 0; j < state->num_recent_files; j++) {
        free(state->recent_files[j]->path);
        free(state->recent_files[j]);
    }
    free(state->recent_files);
    
    if (state->unmatched_brackets) {
        free(state->unmatched_brackets);
        state->unmatched_brackets = NULL;
    }

    if (state->yank_register) free(state->yank_register);
    if (state->move_register) free(state->move_register);
    
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
    gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT; // Layout padrão
}

void ciclar_layout() {
    if (gerenciador.num_janelas <= 1) return;

    switch (gerenciador.num_janelas) {
        case 2:
            if (gerenciador.current_layout == LAYOUT_VERTICAL_SPLIT) {
                gerenciador.current_layout = LAYOUT_HORIZONTAL_SPLIT;
            } else {
                gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        case 3:
            if (gerenciador.current_layout == LAYOUT_VERTICAL_SPLIT) {
                gerenciador.current_layout = LAYOUT_MAIN_AND_STACK;
            } else {
                gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        case 4:
            if (gerenciador.current_layout == LAYOUT_VERTICAL_SPLIT) {
                gerenciador.current_layout = LAYOUT_GRID;
            } else {
                gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        default:
            gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            break;
    }

    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void recalcular_layout_janelas() {
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    if (gerenciador.num_janelas == 0) return;

    if ((gerenciador.num_janelas != 2 && gerenciador.current_layout == LAYOUT_HORIZONTAL_SPLIT) ||
        (gerenciador.num_janelas != 3 && gerenciador.current_layout == LAYOUT_MAIN_AND_STACK) ||
        (gerenciador.num_janelas != 4 && gerenciador.current_layout == LAYOUT_GRID)) {
        gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
    }

    switch (gerenciador.current_layout) {
        case LAYOUT_HORIZONTAL_SPLIT:
            if (gerenciador.num_janelas == 2) {
                int altura_janela = screen_rows / 2;
                for (int i = 0; i < 2; i++) {
                    gerenciador.janelas[i]->y = i * altura_janela;
                    gerenciador.janelas[i]->x = 0;
                    gerenciador.janelas[i]->largura = screen_cols;
                    gerenciador.janelas[i]->altura = (i == 1) ? (screen_rows - altura_janela) : altura_janela;
                }
            }
            break;
        case LAYOUT_MAIN_AND_STACK:
            if (gerenciador.num_janelas == 3) {
                int main_width = screen_cols / 2;
                int stack_width = screen_cols - main_width;
                int stack_height = screen_rows / 2;
                gerenciador.janelas[0]->y = 0;
                gerenciador.janelas[0]->x = 0;
                gerenciador.janelas[0]->largura = main_width;
                gerenciador.janelas[0]->altura = screen_rows;
                gerenciador.janelas[1]->y = 0;
                gerenciador.janelas[1]->x = main_width;
                gerenciador.janelas[1]->largura = stack_width;
                gerenciador.janelas[1]->altura = stack_height;
                gerenciador.janelas[2]->y = stack_height;
                gerenciador.janelas[2]->x = main_width;
                gerenciador.janelas[2]->largura = stack_width;
                gerenciador.janelas[2]->altura = screen_rows - stack_height;
            }
            break;
        case LAYOUT_GRID:
            if (gerenciador.num_janelas == 4) {
                int win_w = screen_cols / 2;
                int win_h = screen_rows / 2;
                gerenciador.janelas[0]->y = 0;     gerenciador.janelas[0]->x = 0;
                gerenciador.janelas[1]->y = 0;     gerenciador.janelas[1]->x = win_w;
                gerenciador.janelas[2]->y = win_h; gerenciador.janelas[2]->x = 0;
                gerenciador.janelas[3]->y = win_h; gerenciador.janelas[3]->x = win_w;
                for(int i=0; i<4; i++) {
                    gerenciador.janelas[i]->altura = (i >= 2) ? screen_rows - win_h : win_h;
                    gerenciador.janelas[i]->largura = (i % 2 != 0) ? screen_cols - win_w : win_w;
                }
            }
            break;
        case LAYOUT_VERTICAL_SPLIT:
        default: {
            int largura_janela = screen_cols / gerenciador.num_janelas;
            for (int i = 0; i < gerenciador.num_janelas; i++) {
                gerenciador.janelas[i]->y = 0;
                gerenciador.janelas[i]->x = i * largura_janela;
                gerenciador.janelas[i]->altura = screen_rows;
                gerenciador.janelas[i]->largura = (i == gerenciador.num_janelas - 1) ? (screen_cols - gerenciador.janelas[i]->x) : largura_janela;
            }
            break;
        }
    }

    for (int i = 0; i < gerenciador.num_janelas; i++) {
        JanelaEditor *jw = gerenciador.janelas[i];
        if (jw->win) {
            delwin(jw->win);
        }
        jw->win = newwin(jw->altura, jw->largura, jw->y, jw->x);
        keypad(jw->win, TRUE);
        scrollok(jw->win, FALSE);
    }
}

void rotacionar_janelas() {
    if (gerenciador.num_janelas <= 1) return;
    JanelaEditor *ultima_janela = gerenciador.janelas[gerenciador.num_janelas - 1];
    for (int i = gerenciador.num_janelas - 1; i > 0; i--) {
        gerenciador.janelas[i] = gerenciador.janelas[i - 1];
    }
    gerenciador.janelas[0] = ultima_janela;
    gerenciador.janela_ativa_idx = (gerenciador.janela_ativa_idx + 1) % gerenciador.num_janelas;
    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void mover_janela_para_posicao(int target_idx) {
    int active_idx = gerenciador.janela_ativa_idx;
    if (gerenciador.num_janelas <= 1 || target_idx < 0 || target_idx >= gerenciador.num_janelas || target_idx == active_idx) return;
    JanelaEditor *janela_ativa_ptr = gerenciador.janelas[active_idx];
    gerenciador.janelas[active_idx] = gerenciador.janelas[target_idx];
    gerenciador.janelas[target_idx] = janela_ativa_ptr;
    gerenciador.janela_ativa_idx = target_idx;
    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
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
    state->num_recent_files = 0;
    state->recent_files = NULL;
    load_file_history(state);
    char initial_cwd[1024];
    if (getcwd(initial_cwd, sizeof(initial_cwd)) != NULL) {
        update_directory_access(state, initial_cwd);
    }
    gerenciador.janelas[gerenciador.num_janelas - 1] = nova_janela;
    gerenciador.janela_ativa_idx = gerenciador.num_janelas - 1;
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

void redesenhar_todas_as_janelas() {
    erase();
    wnoutrefresh(stdscr);
    for (int i = 0; i < gerenciador.num_janelas; i++) {
        JanelaEditor *jw = gerenciador.janelas[i];
        editor_redraw(jw->win, jw->estado);
    }
    if (gerenciador.num_janelas > 0) {
        JanelaEditor* active_jw = gerenciador.janelas[gerenciador.janela_ativa_idx];
        EditorState* state = active_jw->estado;
        if (state->lsp_enabled) {
            LspDiagnostic *diag = get_diagnostic_under_cursor(state);
            if (diag) {
                draw_diagnostic_popup(active_jw->win, state, diag->message);
            }
        }
        state->status_msg[0] = '\0';
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

void fechar_janela_ativa(bool *should_exit) {
    if (gerenciador.num_janelas == 0) return;
    int idx = gerenciador.janela_ativa_idx;
    JanelaEditor *jw = gerenciador.janelas[idx];
    EditorState *state = gerenciador.janelas[gerenciador.janela_ativa_idx]->estado;
    if (state->lsp_enabled) {
        lsp_shutdown(state);
    }
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

bool confirm_action(const char *prompt) {
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    int win_h = 3;
    int win_w = strlen(prompt) + 8;
    int win_y = (rows - win_h) / 2;
    int win_x = (cols - win_w) / 2;
    WINDOW *confirm_win = newwin(win_h, win_w, win_y, win_x);
    wbkgd(confirm_win, COLOR_PAIR(9));
    box(confirm_win, 0, 0);
    mvwprintw(confirm_win, 1, 2, "%s (y/n)", prompt);
    wrefresh(confirm_win);
    wint_t ch;
    keypad(confirm_win, TRUE);
    curs_set(0);
    while(1) {
        wget_wch(confirm_win, &ch);
        if (ch == 'y' || ch == 'Y') {
            delwin(confirm_win);
            return true;
        }
        if (ch == 'n' || ch == 'N' || ch == 27) {
            delwin(confirm_win);
            return false;
        }
    }
}

typedef struct {
    char* path;
    bool is_recent;
} SearchResult;

void display_recent_files() {
    EditorState *active_state = gerenciador.janelas[gerenciador.janela_ativa_idx]->estado;
    
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

        if (search_mode) {
            if (ch == KEY_ENTER || ch == '\n' || ch == 27) {
                search_mode = false;
                curs_set(0);
            } else if (ch == KEY_BACKSPACE || ch == 127) {
                if (search_pos > 0) search_term[--search_pos] = '\0';
            } else if (isprint(ch)) {
                if (search_pos < (int)sizeof(search_term) - 1) {
                    search_term[search_pos++] = ch;
                    search_term[search_pos] = '\0';
                }
            }
            current_selection = 0;
            top_of_list = 0;
            continue;
        }

        switch(ch) {
            case '/':
                search_mode = true;
                curs_set(1);
                break;
            case KEY_UP: case 'k':
                if (current_selection > 0) current_selection--;
                break;
            case KEY_DOWN: case 'j':
                if (current_selection < list_size - 1) current_selection++;
                break;
            case KEY_ENTER: case '\n':
                {
                    if (list_size == 0) goto end_switcher;
                    
                    char* selected_file;
                    if (search_term[0] != '\0') {
                        selected_file = results[current_selection].path;
                    } else {
                        selected_file = active_state->recent_files[current_selection]->path;
                    }

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
                    goto end_switcher;
                }
            case 27: case 'q':
                goto end_switcher;
        }
    }

end_switcher:
    for(int i = 0; i < num_results; i++) free(results[i].path);
    free(results);
    delwin(switcher_win);
    touchwin(stdscr);
    redesenhar_todas_as_janelas();
}