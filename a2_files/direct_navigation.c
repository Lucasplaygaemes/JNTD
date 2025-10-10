#include "direct_navigation.h" // Include its own header
#include "defs.h" // For EditorState, etc.
#include "screen_ui.h" // For redesenhar_todas_as_janelas
#include "window_managment.h" // For redesenhar_todas_as_janelas

#include <limits.h> // For PATH_MAX
#include <unistd.h> // For chdir, getcwd
#include <errno.h> // For errno
#include <ctype.h> // For tolower
#include <stdio.h> // For sscanf, fgets, fopen, fclose
#include <string.h> // For strcmp, strstr, etc.
#include <stdarg.h> // For va_list, etc.

// ===================================================================
// Debug Logging
// ===================================================================

void debug_log(const char *format, ...) {
    FILE *log_file = fopen("/tmp/jntd_debug.log", "a");
    if (log_file) {
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fclose(log_file);
    }
}

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
    debug_log("--- Loading directory history ---\n");
    state->recent_dirs = NULL;
    state->num_recent_dirs = 0;

    char history_file[1024];
    get_history_filename(history_file, sizeof(history_file));
    debug_log("Directory history file path: %s\n", history_file);

    FILE *f = fopen(history_file, "r");
    if (!f) {
        debug_log("Failed to open directory history file: %s\n", strerror(errno));
        return;
    }
    debug_log("Directory history file opened successfully.\n");

    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        debug_log("Read line from dir history: %s", line);
        int count;
        char path[1024];
        int result = sscanf(line, "%d %1023[^\n]", &count, path);
        if (result == 2) {
            debug_log("sscanf successful for dir: count=%d, path=%s\n", count, path);
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
        } else {
            debug_log("sscanf failed for dir, result=%d\n", result);
        }
    }
    fclose(f);
    debug_log("--- Finished loading directory history. Found %d entries. ---\n", state->num_recent_dirs);

    if (state->num_recent_dirs > 0) {
        qsort(state->recent_dirs, state->num_recent_dirs, sizeof(DirectoryInfo*), compare_dirs);
    }
}

void save_directory_history(EditorState *state) {
    char history_file[1024];
    get_history_filename(history_file, sizeof(history_file));

    FILE *f = fopen(history_file, "w");
    if (!f) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Error saving dir history: %s", strerror(errno));
        return;
    }

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

    int current_selection = 0;
    int top_of_list = 0;
    int max_visible = win_h - 4;

    char search_term[100] = {0};
    int search_pos = 0;
    bool search_mode = false;

    DirectoryInfo **filtered_dirs = NULL;
    int num_filtered = 0;

    while (1) {
        if (search_term[0] != '\0') {
            if (filtered_dirs && filtered_dirs != state->recent_dirs) free(filtered_dirs);
            num_filtered = 0;
            filtered_dirs = malloc(sizeof(DirectoryInfo*) * state->num_recent_dirs);
            for (int i = 0; i < state->num_recent_dirs; i++) {
                if (strstr(state->recent_dirs[i]->path, search_term)) {
                    filtered_dirs[num_filtered++] = state->recent_dirs[i];
                }
            }
        } else {
            if (filtered_dirs && filtered_dirs != state->recent_dirs) free(filtered_dirs);
            filtered_dirs = state->recent_dirs;
            num_filtered = state->num_recent_dirs;
        }

        if (current_selection >= num_filtered) {
            current_selection = num_filtered > 0 ? num_filtered - 1 : 0;
        }
        if (top_of_list > current_selection) top_of_list = current_selection;
        if (max_visible > 0 && top_of_list < current_selection - max_visible + 1) {
            top_of_list = current_selection - max_visible + 1;
        }


        werase(nav_win);
        box(nav_win, 0, 0);
        mvwprintw(nav_win, 1, (win_w - 25) / 2, "Navegador de Diretórios");

        for (int i = 0; i < max_visible; i++) {
            int dir_idx = top_of_list + i;
            if (dir_idx < num_filtered) {
                if (dir_idx == current_selection) wattron(nav_win, A_REVERSE);

                char display_path[win_w - 4];
                strncpy(display_path, filtered_dirs[dir_idx]->path, sizeof(display_path) - 1);
                display_path[sizeof(display_path) - 1] = '\0';
                
                if (strlen(filtered_dirs[dir_idx]->path) > sizeof(display_path) - 1) {
                    strcpy(display_path + sizeof(display_path) - 4, "...");
                }
                
                mvwprintw(nav_win, i + 2, 2, "%s (%d acessos)", 
                         display_path, filtered_dirs[dir_idx]->access_count);
                
                if (dir_idx == current_selection) wattroff(nav_win, A_REVERSE);
            }
        }

        mvwprintw(nav_win, win_h - 2, 2, "ESC: Sair | /: Buscar | ENTER: Selecionar");
        mvwprintw(nav_win, win_h - 1, 2, "/%s", search_term);
        if (search_mode) {
            wmove(nav_win, win_h - 1, 3 + search_pos);
            curs_set(1);
        } else {
            curs_set(0);
        }

        wrefresh(nav_win);

        int ch = wgetch(nav_win);
        switch(ch) {
            case '/':
                search_mode = true;
                break;
            case KEY_RESIZE:
                getmaxyx(stdscr, rows, cols);
                win_h = max(state->num_recent_dirs + 4, 10);
                win_h = min(win_h, rows - 4);
                win_w = cols - 10;
                if (win_w < 50) win_w = 50;
                win_y = (rows - win_h) / 2;
                win_x = (cols - win_w) / 2;

                wresize(nav_win, win_h, win_w);
                mvwin(nav_win, win_y, win_x);

                touchwin(stdscr);
                redesenhar_todas_as_janelas();
                break;
            case KEY_UP:
                if (current_selection > 0) {
                    current_selection--;
                    if(current_selection < top_of_list) top_of_list = current_selection;
                }
                break;
            case KEY_DOWN:
                if (current_selection < num_filtered - 1) {
                    current_selection++;
                    if(current_selection >= top_of_list + max_visible) top_of_list = current_selection - max_visible + 1;
                }
                break;
            case KEY_ENTER: case '\n': case '\r':
                if (search_mode) {
                    search_mode = false;
                } else if (num_filtered > 0) {
                    change_directory(state, filtered_dirs[current_selection]->path);
                    goto end_nav;
                }
                break;
            case 27: // ESC
                if (search_mode) {
                    search_mode = false;
                    search_term[0] = '\0';
                    search_pos = 0;
                    current_selection = 0;
                    top_of_list = 0;
                } else {
                    goto end_nav;
                }
                break;
            case KEY_BACKSPACE: case 127:
                if (search_mode && search_pos > 0) {
                    search_term[--search_pos] = '\0';
                    current_selection = 0;
                    top_of_list = 0;
                }
                break;
            case KEY_NPAGE:
                current_selection = min(current_selection + max_visible, num_filtered - 1);
                top_of_list = min(top_of_list + max_visible, num_filtered - max_visible);
                if (top_of_list < 0) top_of_list = 0;
                break;
            case KEY_PPAGE:
                current_selection = max(current_selection - max_visible, 0);
                top_of_list = max(top_of_list - max_visible, 0);
                break;
            default:
                if (search_mode && isprint(ch) && search_pos < (int)sizeof(search_term) - 1) {
                    search_term[search_pos++] = ch;
                    search_term[search_pos] = '\0';
                    current_selection = 0;
                    top_of_list = 0;
                }
        }
    }

end_nav:
    if (search_term[0] != '\0') {
        free(filtered_dirs);
    }
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
    debug_log("--- Loading file history ---\n");
    for (int i = 0; i < state->num_recent_files; i++) {
        free(state->recent_files[i]->path);
        free(state->recent_files[i]);
    }
    free(state->recent_files);
    state->recent_files = NULL;
    state->num_recent_files = 0;
    
    char history_file[1024];
    get_file_history_filename(history_file, sizeof(history_file));
    debug_log("File history path: %s\n", history_file);

    FILE *f = fopen(history_file, "r");
    if (!f) {
        debug_log("Failed to open file history: %s\n", strerror(errno));
        return;
    }
    debug_log("File history opened successfully.\n");
    
    char line[MAX_LINE_LEN];
    while (fgets(line, sizeof(line), f)) {
        debug_log("Read line from file history: %s", line);
        int count;
        char path[4096];
        int result = sscanf(line, "%d %4095[^\n]", &count, path);
        if (result == 2) {
            debug_log("sscanf successful for file: count=%d, path=%s\n", count, path);
            FileInfo *new_file = malloc(sizeof(FileInfo));
            if (!new_file) continue;
            
            new_file->path = strdup(path);
            if (!new_file->path) { free(new_file); continue; }
            
            new_file->access_count = count;

            state->num_recent_files++;
            state->recent_files = realloc(state->recent_files, sizeof(FileInfo*) * state->num_recent_files);
            state->recent_files[state->num_recent_files - 1] = new_file;
        } else {
            debug_log("sscanf failed for file, result=%d\n", result);
        }
    }
    fclose(f);
    debug_log("--- Finished loading file history. Found %d entries. ---\n", state->num_recent_files);
    
    qsort(state->recent_files, state->num_recent_files, sizeof(FileInfo*), compare_files);
}

void save_file_history(EditorState *state) {
    char history_file[1024];
    get_file_history_filename(history_file, sizeof(history_file));
    
    FILE *f = fopen(history_file, "w");
    if (!f) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Error saving file history: %s", strerror(errno));
        return;
    }
    
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
    if (!new_file || !state->recent_files) { // This condition seems to be checking against state->recent_files, which might be incorrect if realloc succeeded but new_file failed.
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