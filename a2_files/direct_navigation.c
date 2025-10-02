#include "direct_navigation.h" // Include its own header
#include "defs.h" // For EditorState, etc.
#include "screen_ui.h" // For redesenhar_todas_as_janelas
#include "window_managment.h" // For redesenhar_todas_as_janelas

#include <limits.h> // For PATH_MAX
#include <unistd.h> // For chdir, getcwd
#include <errno.h> // For errno
#include <ctype.h> // For tolower
#include <stdio.h> // For sscanf, fgets, fopen, fclose

// ===================================================================
// 4. Directory Navigation
// ===================================================================

void get_history_filename(char *buffer, size_t size) {
    const char *home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(buffer, size, "%s/.jntd_dir_history", home_dir);
    }
    else {
        snprintf(buffer, size, ".jntd_dir_history");
    }
}

int compare_dirs(const void *a, const void *b) {
    DirectoryInfo *dir_a = *(DirectoryInfo**)a;
    DirectoryInfo *dir_b = *(DirectoryInfo**)b;
    return dir_b->access_count - dir_a->access_count;
}

void load_directory_history(EditorState *state) {
    state->recent_dirs = NULL;
    state->num_recent_dirs = 0;

    char history_file[1024];
    get_history_filename(history_file, sizeof(history_file));

    FILE *f = fopen(history_file, "r");
    if (!f) return;

    char line[MAX_LINE_LEN];
    const char* sscanf_format = "%d %1023[^]";
    while (fgets(line, sizeof(line), f)) {
        int count;
        char path[1024];
        if (sscanf(line, sscanf_format, &count, path) == 2) {
            DirectoryInfo *new_dir = malloc(sizeof(DirectoryInfo));
            if (!new_dir) continue;
            
            new_dir->path = strdup(path);
            if (!new_dir->path) { free(new_dir); continue; }
            
            new_dir->access_count = count;

            state->num_recent_dirs++;
            state->recent_dirs = realloc(state->recent_dirs, sizeof(DirectoryInfo*) * state->num_recent_dirs);
            if (!state->recent_dirs) {
                free(new_dir->path);
                free(new_dir);
                state->num_recent_dirs--;
                break;
            }
            
            state->recent_dirs[state->num_recent_dirs - 1] = new_dir;
        }
    }
    fclose(f);

    if (state->num_recent_dirs > 0) {
        qsort(state->recent_dirs, state->num_recent_dirs, sizeof(DirectoryInfo*), compare_dirs);
    }
}

void save_directory_history(EditorState *state) {
    char history_file[1024];
    get_history_filename(history_file, sizeof(history_file));

    FILE *f = fopen(history_file, "w");
    if (!f) return;

    for (int i = 0; i < state->num_recent_dirs; i++) {
        fprintf(f, "%d %s\n", state->recent_dirs[i]->access_count, state->recent_dirs[i]->path);
    }
    fclose(f);
}

void update_directory_access(EditorState *state, const char *path) {
    char canonical_path[PATH_MAX];
    if (realpath(path, canonical_path) == NULL) {
        strncpy(canonical_path, path, sizeof(canonical_path)-1);
        canonical_path[sizeof(canonical_path)-1] = '\0';
    }

    for (int i = 0; i < state->num_recent_dirs; i++) {
        if (strcmp(state->recent_dirs[i]->path, canonical_path) == 0) {
            state->recent_dirs[i]->access_count++;
            qsort(state->recent_dirs, state->num_recent_dirs, sizeof(DirectoryInfo*), compare_dirs);
            save_directory_history(state);
            return;
        }
    }

    state->num_recent_dirs++;
    state->recent_dirs = realloc(state->recent_dirs, sizeof(DirectoryInfo*) * state->num_recent_dirs);

    DirectoryInfo *new_dir = malloc(sizeof(DirectoryInfo));
    new_dir->path = strdup(canonical_path);
    new_dir->access_count = 1;

    state->recent_dirs[state->num_recent_dirs - 1] = new_dir;

    qsort(state->recent_dirs, state->num_recent_dirs, sizeof(DirectoryInfo*), compare_dirs);
    save_directory_history(state);
}

void change_directory(EditorState *state, const char *new_path) {
    if (chdir(new_path) == 0) {
        update_directory_access(state, new_path);
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Diretório mudado para: %s", cwd);
        }
    }
    else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Erro ao mudar para: %s", strerror(errno));
    }
}

void display_directory_navigator(EditorState *state) {
    if (state->num_recent_dirs == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "No recent directories available.");
        return;
    }

    WINDOW *nav_win;
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int win_h = max(state->num_recent_dirs + 4, 10); 
    win_h = min(win_h, rows - 4); 
    int win_w = cols - 10;
    if (win_w < 50) win_w = 50;
    int win_y = (rows - win_h) / 2;
    int win_x = (cols - win_w) / 2;

    nav_win = newwin(win_h, win_w, win_y, win_x);
    if (!nav_win) return;
    
    keypad(nav_win, TRUE);
    wbkgd(nav_win, COLOR_PAIR(9));
    box(nav_win, 0, 0);

    mvwprintw(nav_win, 1, (win_w - 25) / 2, "Navegador de Diretórios");
    
    int current_selection = 0;
    int top_of_list = 0;
    int max_visible = win_h - 4;

    while (1) {
        werase(nav_win);
        box(nav_win, 0, 0);
        mvwprintw(nav_win, 1, (win_w - 25) / 2, "Navegador de Diretórios");

        for (int i = 0; i < max_visible; i++) {
            int dir_idx = top_of_list + i;
            if (dir_idx < state->num_recent_dirs) {
                if (dir_idx == current_selection) wattron(nav_win, A_REVERSE);
                
                char display_path[win_w - 4];
                strncpy(display_path, state->recent_dirs[dir_idx]->path, sizeof(display_path) - 1);
                display_path[sizeof(display_path) - 1] = '\0';
                
                if (strlen(state->recent_dirs[dir_idx]->path) > sizeof(display_path) - 1) {
                    strcpy(display_path + sizeof(display_path) - 4, "...");
                }
                
                mvwprintw(nav_win, i + 2, 2, "%s (%d acessos)", 
                         display_path, state->recent_dirs[dir_idx]->access_count);
                
                if (dir_idx == current_selection) wattroff(nav_win, A_REVERSE);
            }
        }

        mvwprintw(nav_win, win_h - 2, 2, "Use as setas para navegar, ENTER para selecionar, ESC para sair.");
        wrefresh(nav_win);

        int ch = wgetch(nav_win);
        switch(ch) {
            case KEY_UP:
                if (current_selection > 0) {
                    current_selection--;
                    if(current_selection < top_of_list) top_of_list = current_selection;
                }
                break;
            case KEY_DOWN:
                if (current_selection < state->num_recent_dirs - 1) {
                    current_selection++;
                    if(current_selection >= top_of_list + max_visible) top_of_list = current_selection - max_visible + 1;
                }
                break;
            case KEY_ENTER: case '\n': case '\r':
                if (current_selection < state->num_recent_dirs) {
                    change_directory(state, state->recent_dirs[current_selection]->path);
                    goto end_nav;
                }
                break;
            case 27: case 'q': case 'Q': goto end_nav;
            case KEY_NPAGE:
                current_selection = min(current_selection + max_visible, state->num_recent_dirs - 1);
                top_of_list = min(top_of_list + max_visible, state->num_recent_dirs - max_visible);
                break;
            case KEY_PPAGE:
                current_selection = max(current_selection - max_visible, 0);
                top_of_list = max(top_of_list - max_visible, 0);
                break;
        }
    }

end_nav:
    delwin(nav_win);
    touchwin(stdscr);
    redesenhar_todas_as_janelas();
}

void prompt_for_directory_change(EditorState *state) {
    if (state->buffer_modified) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Unsaved changes. Proceed with directory change? (y/n)");
        redesenhar_todas_as_janelas();
        wint_t ch;
        wget_wch(ACTIVE_WS->janelas[ACTIVE_WS->janela_ativa_idx]->win, &ch);
        if (tolower(ch) != 'y') {
            snprintf(state->status_msg, sizeof(state->status_msg), "Cancelled.");
            redesenhar_todas_as_janelas();
            return;
        }
    }

    int rows, cols; getmaxyx(stdscr, rows, cols);
    int win_h = 5; int win_w = cols - 20; if (win_w < 50) win_w = 50;
    int win_y = (rows - win_h) / 2; int win_x = (cols - win_w) / 2;
    WINDOW *input_win = newwin(win_h, win_w, win_y, win_x);
    keypad(input_win, TRUE);
    wbkgd(input_win, COLOR_PAIR(9));
    box(input_win, 0, 0);

    mvwprintw(input_win, 1, 2, "Change to directory:");
    wrefresh(input_win);

    char path_buffer[1024] = {0};
    curs_set(1); echo(); 
    wmove(input_win, 2, 2);
    wgetnstr(input_win, path_buffer, sizeof(path_buffer) - 1);
    noecho(); curs_set(0);

    delwin(input_win);
    touchwin(stdscr);

    if (strlen(path_buffer) > 0) {
        change_directory(state, path_buffer);
    }
    else {
        snprintf(state->status_msg, sizeof(state->status_msg), "No path entered. Cancelled.");
    }
    
    redesenhar_todas_as_janelas();
}

void get_file_history_filename(char *buffer, size_t size) {
    const char *home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(buffer, size, "%s/.jntd_file_history", home_dir);
    } else {
        snprintf(buffer, size, ".jntd_file_history");
    }
}


int compare_files(const void *a, const void *b) {
    FileInfo *file_a = *(FileInfo**)a;
    FileInfo *file_b = *(FileInfo**)b;
    return file_b->access_count - file_a->access_count;
}

void load_file_history(EditorState *state) {
    for (int i = 0; i < state->num_recent_files; i++) {
        free(state->recent_files[i]->path);
        free(state->recent_files[i]);
    }
    free(state->recent_files);
    state->recent_files = NULL;
    state->num_recent_files = 0;
    
    char history_file[1024];
    get_file_history_filename(history_file, sizeof(history_file));
    FILE *f = fopen(history_file, "r");
    if (!f) return;
    
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        int count;
        char path[4096];
        if (sscanf(line, "%d %4095[^]", &count, path) == 2) {
            FileInfo *new_file = malloc(sizeof(FileInfo));
            if (!new_file) continue;
            
            new_file->path = strdup(path);
            if (!new_file->path) { free(new_file); continue; }
            
            new_file->access_count = count;

            state->num_recent_files++;
            state->recent_files = realloc(state->recent_files, sizeof(FileInfo*) * state->num_recent_files);
            state->recent_files[state->num_recent_files - 1] = new_file;
        }
    }
    fclose(f);
    
    qsort(state->recent_files, state->num_recent_files, sizeof(FileInfo*), compare_files);
}

void save_file_history(EditorState *state) {
    char history_file[1024];
    get_file_history_filename(history_file, sizeof(history_file));
    
    FILE *f = fopen(history_file, "w");
    if (!f) return;
    
    int limit = state->num_recent_files < 100 ? state->num_recent_files : 100;
    for (int i = 0; i < limit; i++) {
        fprintf(f, "%d %s\n", state->recent_files[i]->access_count, state->recent_files[i]->path);
    }
    fclose(f);
}

void add_to_file_history(EditorState *state, const char *path) {
    if (strcmp(path, "[No Name]") == 0) return;

    char canonical_path[PATH_MAX];
    if (realpath(path, canonical_path) == NULL) {
        strncpy(canonical_path, path, sizeof(canonical_path)-1);
        canonical_path[sizeof(canonical_path)-1] = '\0';
    }

    // Verifica se o arquivo já existe e incrementa a contagem de acesso
    for (int i = 0; i < state->num_recent_files; i++) {
        if (strcmp(state->recent_files[i]->path, canonical_path) == 0) {
            state->recent_files[i]->access_count++;
            qsort(state->recent_files, state->num_recent_files, sizeof(FileInfo*), compare_files);
            save_file_history(state);
            return;
        }
    }

    // Se não for encontrado, adiciona um novo com contagem 1
    state->num_recent_files++;
    state->recent_files = realloc(state->recent_files, sizeof(FileInfo*) * state->num_recent_files);

    FileInfo *new_file = malloc(sizeof(FileInfo));
    if (!new_file || !state->recent_files) {
        // Lida com falha de alocação
        state->num_recent_files--;
        return;
    }
    
    new_file->path = strdup(canonical_path);
    new_file->access_count = 1;

    state->recent_files[state->num_recent_files - 1] = new_file;

    qsort(state->recent_files, state->num_recent_files, sizeof(FileInfo*), compare_files);
    save_file_history(state);
}
