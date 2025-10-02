#include "fileio.h" // Include its own header
#include "defs.h" // For EditorState, etc.
#include "screen_ui.h" // For editor_redraw, display_output_screen
#include "lsp_client.h" // For lsp_did_save
#include "others.h" // For trim_whitespace, editor_find_unmatched_brackets
#include "command_execution.h" // For run_and_display_command
#include "direct_navigation.h"

#include <limits.h> // For PATH_MAX
#include <errno.h> // For errno, ENOENT
#include <sys/stat.h> // For struct stat, stat
#include <ctype.h> // For tolower
#include <stdio.h> // For sscanf, fgets, fopen, fclose
#include <string.h> // For strncpy, strlen, strchr, strrchr, strcmp, strcspn
#include <stdlib.h> // For realpath, calloc, free, realloc

// ===================================================================
// 3. File I/O & Handling
// ===================================================================

// Helper function to determine syntax file based on extension
const char * get_syntax_file_from_extension(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return NULL; // Default to no syntax highlighting
    
    if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0)
        return "c.syntax";
    else if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".hpp") == 0)
        return "cpp.syntax";
    else if (strcmp(ext, ".py") == 0)
        return "python.syntax";
    else if (strcmp(ext, ".php") == 0)
        return "php.syntax";
    else if (strcmp(ext, ".js") == 0)
        return "javascript.syntax";
    else if (strcmp(ext, ".java") == 0)
        return "java.syntax";
    else if (strcmp(ext, ".ts") == 0)
        return "typescript.syntax";
    else if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0)
        return "html.syntax";
    else if (strcmp(ext, ".css") == 0)
        return "css.syntax";
    else if (strcmp(ext, ".rb") == 0)
        return "ruby.syntax";
    else if (strcmp(ext, ".rs") == 0)
        return "rust.syntax";
    else if (strcmp(ext, ".go") == 0)
        return "go.syntax";
    
    return NULL; // Default to no syntax highlighting for unknown extensions
}

void load_file_core(EditorState *state, const char *filename) {
    for (int i = 0; i < state->num_lines; i++) { if(state->lines[i]) free(state->lines[i]); state->lines[i] = NULL; }
    state->num_lines = 0;
    strncpy(state->filename, filename, sizeof(state->filename) - 1);
    state->filename[sizeof(state->filename) - 1] = '\0';


    char absolute_path[PATH_MAX];
    if (realpath(filename, absolute_path) == NULL) {
        // Se realpath falhar, use o filename original
        strncpy(absolute_path, filename, PATH_MAX - 1);
        absolute_path[PATH_MAX - 1] = '\0';
    }

    for (int i = 0; i < state->num_lines; i++) { if(state->lines[i]) free(state->lines[i]); state->lines[i] = NULL; } // This line was duplicated and caused issues
    state->num_lines = 0;
    strncpy(state->filename, absolute_path, sizeof(state->filename) - 1);
    state->filename[sizeof(state->filename) - 1] = '\0';



    FILE *file = fopen(filename, "r");
    if (file) {
        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), file) && state->num_lines < MAX_LINES) {
            line[strcspn(line, "\n")] = 0;
            state->lines[state->num_lines] = strdup(line);
            if (!state->lines[state->num_lines]) { fclose(file); return; }
            state->num_lines++;
        }
        fclose(file);
        snprintf(state->status_msg, sizeof(state->status_msg), "%s loaded", filename);
    } else {
        if (errno == ENOENT) {
            state->lines[0] = calloc(1, 1);
            if (!state->lines[0]) return; 
            state->num_lines = 1;
            snprintf(state->status_msg, sizeof(state->status_msg), "New file: \"%s\"", filename);
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "Error opening file: %s", strerror(errno));
        }
    }

    if (state->num_lines == 0) {
        state->lines[0] = calloc(1, 1);
        state->num_lines = 1;
    }
    state->current_line = load_last_line(filename);
    if (state->current_line >= state->num_lines) {
        state->current_line = state->num_lines > 0 ? state->num_lines - 1 : 0;
    }
    if (state->current_line < 0) {
        state->current_line = 0;
    }
    state->current_col = 0;
    state->ideal_col = 0;
    state->top_line = 0;
    state->left_col = 0;
    state->buffer_modified = false;
    state->last_file_mod_time = get_file_mod_time(state->filename);
    editor_find_unmatched_brackets(state);
}
   
void load_file(EditorState *state, const char *filename) {
    add_to_file_history(state, filename);
	if (strstr(filename, AUTO_SAVE_EXTENSION) == NULL) {
		char sv_filename[256];
		snprintf(sv_filename, sizeof(sv_filename), "%s%s", filename, AUTO_SAVE_EXTENSION);
		struct stat st;
		if (stat(sv_filename, &st) == 0) {
		    handle_file_recovery(state, filename, sv_filename);
		    return;
		}
	}
	load_file_core(state, filename);
        const char * syntax_file = get_syntax_file_from_extension(filename);
        load_syntax_file(state, syntax_file);
}

void save_file(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) { 
        strncpy(state->status_msg, "No file name. Use :w <filename>", sizeof(state->status_msg) - 1); 
        return; 
    } 
    
    FILE *file = fopen(state->filename, "w");
    if (file) {
        for (int i = 0; i < state->num_lines; i++) {
            if (state->lines[i]) {
                fprintf(file, "%s\n", state->lines[i]);
            }
        }
        fclose(file); 
        
        char auto_save_filename[256];
        snprintf(auto_save_filename, sizeof(auto_save_filename), "%s%s", state->filename, AUTO_SAVE_EXTENSION);
        remove(auto_save_filename);
        
        char display_filename[40]; 
        strncpy(display_filename, state->filename, sizeof(display_filename) - 1);
        display_filename[sizeof(display_filename) - 1] = '\0';
        snprintf(state->status_msg, sizeof(state->status_msg), "%s written", display_filename);
        state->buffer_modified = false;
        state->last_file_mod_time = get_file_mod_time(state->filename);
        if (state->lsp_enabled) {
          lsp_did_save(state);
            }
    } else { 
        snprintf(state->status_msg, sizeof(state->status_msg), "Error saving: %s", strerror(errno)); 
    } 
}

void auto_save(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) return;
    if (!state->buffer_modified) return;

    char auto_save_filename[256];
    snprintf(auto_save_filename, sizeof(auto_save_filename), "%s%s", state->filename, AUTO_SAVE_EXTENSION);

    FILE *file = fopen(auto_save_filename, "w");
    if (file) {
        for (int i = 0; i < state->num_lines; i++) {
            if (state->lines[i]) {
                fprintf(file, "%s\n", state->lines[i]);
            }
        }
        fclose(file);
    }
}

time_t get_file_mod_time(const char *filename) {
    struct stat attr;
    if (stat(filename, &attr) == 0) {
        return attr.st_mtime;
    }
    return 0;
}

void check_external_modification(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0 || state->last_file_mod_time == 0) {
        return;
    }

    time_t on_disk_mod_time = get_file_mod_time(state->filename);

    if (on_disk_mod_time != 0 && on_disk_mod_time != state->last_file_mod_time) {
        WINDOW *win = gerenciador.janelas[gerenciador.janela_ativa_idx]->win;
        if (state->buffer_modified) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Warning: File on disk has changed! (S)ave, (L)oad, or (C)ancel?");
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "File on disk has changed. Reload? (Y/N)");
        }
        editor_redraw(win, state);
        wint_t ch;
        wget_wch(win, &ch);
        if (state->buffer_modified) {
             switch (tolower(ch)) {
                case 's':
                    save_file(state);
                    break;
                case 'l':
                    load_file(state, state->filename);
                    break;
                default:
                    state->last_file_mod_time = on_disk_mod_time;
                    snprintf(state->status_msg, sizeof(state->status_msg), "Action cancelled. In-memory version kept.");
                    break;
            }
        } else {
            if (tolower(ch) == 'y') {
                load_file(state, state->filename);
            } else {
                state->last_file_mod_time = on_disk_mod_time;
                 snprintf(state->status_msg, sizeof(state->status_msg), "Reload cancelled.");
            }
        }
    }
}

void editor_reload_file(EditorState *state) {
    if (strcmp(state->filename, "[No Name]") == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "No file name to reload.");
        return;
    }
    
    time_t on_disk_mod_time = get_file_mod_time(state->filename);
    
    if (state->buffer_modified && on_disk_mod_time != 0 && on_disk_mod_time != state->last_file_mod_time) {
        // Usar mensagem de status em vez de diálogo interativo
        snprintf(state->status_msg, sizeof(state->status_msg), 
                 "Warning: File changed on disk! Use :rc! to force reload.");
        return;
    }
    
    // Recarregar o arquivo normalmente
    load_file(state, state->filename);
    snprintf(state->status_msg, sizeof(state->status_msg), "File reloaded.");
}

void load_syntax_file(EditorState *state, const char *filename) {
    // Limpa as regras de sintaxe existentes antes de carregar novas
    if (state->syntax_rules) {
        for (int i = 0; i < state->num_syntax_rules; i++) {
            free(state->syntax_rules[i].word);
        }
        free(state->syntax_rules);
        state->syntax_rules = NULL;
        state->num_syntax_rules = 0;
    }

    if (!filename) {
        return; // No syntax file to load, just clear old rules.
    }

    char path[PATH_MAX];
    // If the executable path is known, construct an absolute path to syntaxes
    if (executable_dir[0] != '\0') {
        snprintf(path, sizeof(path), "%s/syntaxes/%s", executable_dir, filename);
    } else {
        // Fallback to the old relative path method
        snprintf(path, sizeof(path), "syntaxes/%s", filename);
    }

    FILE *file = fopen(path, "r");
    if (!file) {
        // Fallback to current directory if not found in syntaxes/
        file = fopen(filename, "r");
        if (!file) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Erro: sintaxe '%s' nao encontrada.", filename);
            return;
        }
    }

    char line_buffer[256];
    while (fgets(line_buffer, sizeof(line_buffer), file)) {
        if (line_buffer[0] == '#' || line_buffer[0] == '\n' || line_buffer[0] == '\r') continue;

        line_buffer[strcspn(line_buffer, "\r\n")] = 0;

        char *colon = strchr(line_buffer, ':');
        if (!colon) continue;

        *colon = '\0'; 
        
        char *type_str = trim_whitespace(line_buffer);
        char *word_str = trim_whitespace(colon + 1);

        if (strlen(type_str) == 0 || strlen(word_str) == 0) continue;

        state->num_syntax_rules++;
        state->syntax_rules = realloc(state->syntax_rules, sizeof(SyntaxRule) * state->num_syntax_rules);
        
        SyntaxRule *new_rule = &state->syntax_rules[state->num_syntax_rules - 1];
        new_rule->word = strdup(word_str);

        if (strcmp(type_str, "KEYWORD") == 0) {
            new_rule->type = SYNTAX_KEYWORD;
        } else if (strcmp(type_str, "TYPE") == 0) {
            new_rule->type = SYNTAX_TYPE;
        } else if (strcmp(type_str, "STD_FUNCTION") == 0) {
            new_rule->type = SYNTAX_STD_FUNCTION;
        } else {
            free(new_rule->word);
            state->num_syntax_rules--;
        }
    }
    fclose(file);
}

void save_last_line(const char *filename, int line) {
    char pos_filename[256];
    snprintf(pos_filename, sizeof(pos_filename), "%s.pos", filename);
    FILE *f = fopen(pos_filename, "w");
    if (f) {
        fprintf(f, "%d", line);
        fclose(f);
    }
}

int load_last_line(const char *filename) {
    char pos_filename[256];
    snprintf(pos_filename, sizeof(pos_filename), "%s.pos", filename);
    FILE *f = fopen(pos_filename, "r");
    if (f) {
        int line = 0;
        fscanf(f, "%d", &line);
        fclose(f);
    }
    return 0;
}

// ===================================================================
// 3. File Recovery
// ===================================================================

FileRecoveryChoice display_recovery_prompt(WINDOW *parent_win, EditorState *state) {
    int rows, cols;
    getmaxyx(parent_win, rows, cols);
    
    // Criar uma janela de diálogo
    int win_height = 7;
    int win_width = 50;
    int start_y = (rows - win_height) / 2;
    int start_x = (cols - win_width) / 2;
    
    WINDOW *dialog_win = newwin(win_height, win_width, start_y, start_x);
    keypad(dialog_win, TRUE);
    wbkgd(dialog_win, COLOR_PAIR(9));
    box(dialog_win, 0, 0);
    
    // Exibir mensagem
    mvwprintw(dialog_win, 1, 2, "Arquivo de recuperação encontrado!");
    mvwprintw(dialog_win, 2, 2, "Escolha uma opção:");
    mvwprintw(dialog_win, 3, 4, "(R)ecover from .sv");
    mvwprintw(dialog_win, 4, 4, "(O)pen original");
    mvwprintw(dialog_win, 5, 4, "(D)iff files");
    mvwprintw(dialog_win, 6, 4, "(I)gnore | (Q)uit");
    
    wrefresh(dialog_win);
    
    FileRecoveryChoice choice = RECOVER_ABORT;
    bool decided = false;
    
    while (!decided) {
        wint_t ch;
        wget_wch(dialog_win, &ch);
        ch = tolower(ch);
        
        switch (ch) {
            case 'r': choice = RECOVER_FROM_SV; decided = true; break;
            case 'o': choice = RECOVER_OPEN_ORIGINAL; decided = true; break;
            case 'd': choice = RECOVER_DIFF; decided = true; break;
            case 'i': choice = RECOVER_IGNORE; decided = true; break;
            case 'q': case 27: choice = RECOVER_ABORT; decided = true; break;
        }
    }
    
    delwin(dialog_win);
    return choice;
}

void handle_file_recovery(EditorState *state, const char *original_filename, const char *sv_filename) {
    WINDOW *win = gerenciador.janelas[gerenciador.janela_ativa_idx]->win;
    
    while (1) {
        FileRecoveryChoice choice = display_recovery_prompt(win, state);
        
        switch (choice) {
            case RECOVER_DIFF: {
                char diff_command[1024];
                snprintf(diff_command, sizeof(diff_command), "git diff --no-index -- %s %s", original_filename, sv_filename);
                run_and_display_command(diff_command, "--- DIFERENÇAS ---");
                break; 
            }
            case RECOVER_FROM_SV:
                load_file_core(state, sv_filename);
                strncpy(state->filename, original_filename, sizeof(state->filename) - 1);
                state->buffer_modified = true;
                remove(sv_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Recuperado de %s. Salve para confirmar.", sv_filename);
                return;

            case RECOVER_OPEN_ORIGINAL:
                remove(sv_filename);
                load_file_core(state, original_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Arquivo de recuperação ignorado e removido.");
                return;

            case RECOVER_IGNORE:
                load_file_core(state, original_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Arquivo de recuperação mantido.");
                return;

            case RECOVER_ABORT:
                state->status_msg[0] = '\0';
                state->num_lines = 1;
                state->lines[0] = calloc(1, 1);
                strcpy(state->filename, "[No Name]");
                return;
        }
    }
}
