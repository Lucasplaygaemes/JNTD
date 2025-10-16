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
#include <libgen.h> // For dirname()
#include <unistd.h> // For getcwd()

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
        // If realpath fails, use the original filename
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

    // Add the file's directory to the directory history
    char *path_copy = strdup(filename);
    if (path_copy) {
        char *dir = dirname(path_copy);
        if (dir) {
            if (strcmp(dir, ".") == 0) {
                char cwd[PATH_MAX];
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    update_directory_access(state, cwd);
                }
            } else {
                update_directory_access(state, dir);
            }
        }
        free(path_copy);
    }

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
        bool decision = false;
        if (state->buffer_modified) {
            decision = confirm_action("File on disk changed! Discard your changes and reload?");
        } else {
            decision = confirm_action("File on disk changed. Reload?");
        }

        if (decision) {
            // Force reloading the file from disk
            load_file_core(state, state->filename);
            const char* syntax_file = get_syntax_file_from_extension(state->filename);
            load_syntax_file(state, syntax_file);
            snprintf(state->status_msg, sizeof(state->status_msg), "File reloaded from disk.");
        } else {
            // If the user chooses "no", we just update the modification time
            // to avoid asking again, keeping the in-memory version.
            state->last_file_mod_time = on_disk_mod_time;
            snprintf(state->status_msg, sizeof(state->status_msg), "Reload cancelled. In-memory version kept.");
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
        // Use status message instead of interactive dialog
        snprintf(state->status_msg, sizeof(state->status_msg), 
                 "Warning: File changed on disk! Use :rc! to force reload.");
        return;
    }
    
    // Reload the file normally
    load_file(state, state->filename);
    snprintf(state->status_msg, sizeof(state->status_msg), "File reloaded.");
}

void load_syntax_file(EditorState *state, const char *filename) {
    // Clear existing syntax rules before loading new ones
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
            snprintf(state->status_msg, sizeof(state->status_msg), "Error: syntax '%s' not found.", filename);
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
    
    // Create a dialog window
    int win_height = 7;
    int win_width = 50;
    int start_y = (rows - win_height) / 2;
    int start_x = (cols - win_width) / 2;
    
    WINDOW *dialog_win = newwin(win_height, win_width, start_y, start_x);
    keypad(dialog_win, TRUE);
    wbkgd(dialog_win, COLOR_PAIR(9));
    box(dialog_win, 0, 0);
    
    // Display message
    mvwprintw(dialog_win, 1, 2, "Recovery file found!");
    mvwprintw(dialog_win, 2, 2, "Choose an option:");
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
    WINDOW *win = ACTIVE_WS->janelas[ACTIVE_WS->janela_ativa_idx]->win;
    
    while (1) {
        FileRecoveryChoice choice = display_recovery_prompt(win, state);
        
        switch (choice) {
            case RECOVER_DIFF: {
                char diff_command[1024];
                snprintf(diff_command, sizeof(diff_command), "git diff --no-index -- %s %s", original_filename, sv_filename);
                run_and_display_command(diff_command, "--- DIFFERENCES ---");
                break; 
            }
            case RECOVER_FROM_SV:
                load_file_core(state, sv_filename);
                strncpy(state->filename, original_filename, sizeof(state->filename) - 1);
                state->buffer_modified = true;
                remove(sv_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Recovered from %s. Save to confirm.", sv_filename);
                return;

            case RECOVER_OPEN_ORIGINAL:
                remove(sv_filename);
                load_file_core(state, original_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Recovery file ignored and removed.");
                return;

            case RECOVER_IGNORE:
                load_file_core(state, original_filename);
                snprintf(state->status_msg, sizeof(state->status_msg), "Recovery file kept.");
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

// Helper to get the path for the macros file
void get_macros_filename(char *buffer, size_t size) {
    const char *home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(buffer, size, "%s/.a2_macros", home_dir);
    } else {
        // Fallback if HOME is not set
        snprintf(buffer, size, ".a2_macros");
    }
}

void save_macros(EditorState *state) {
    char path[PATH_MAX];
    get_macros_filename(path, sizeof(path));

    FILE *f = fopen(path, "w");
    if (!f) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Error saving macros: %s", strerror(errno));
        return;
    }

    for (int i = 0; i < 26; i++) {
        if (state->macro_registers[i]) {
            char reg = 'a' + i;
            char *content = state->macro_registers[i];
            fprintf(f, "%c:", reg);
            // Escape and write content
            for (size_t j = 0; j < strlen(content); j++) {
                char c = content[j];
                if (c == '\\') {
                    fputs("\\\\", f);
                } else if (c == '\n') {
                    fputs("\\n", f);
                } else if (!isprint(c)) {
                    fprintf(f, "\\x%02x", (unsigned char)c);
                } else {
                    fputc(c, f);
                }
            }
            fputc('\n', f);
        }
    }
    fclose(f);
    snprintf(state->status_msg, sizeof(state->status_msg), "Macros saved to %s", path);
}

void load_macros(EditorState *state) {
    char path[PATH_MAX];
    FILE *f = NULL;

    // 1. Try to load from the user's home directory first.
    const char *home_dir = getenv("HOME");
    if (home_dir) {
        snprintf(path, sizeof(path), "%s/.a2_macros", home_dir);
        f = fopen(path, "r");
    }

    // 2. If not found in home, try the current working directory (project root).
    if (!f) {
        snprintf(path, sizeof(path), ".a2_macros");
        f = fopen(path, "r");
    }

    // 3. If still not found, do nothing.
    if (!f) {
        // File doesn't exist in either location, which is fine.
        return;
    }

    // Clear existing macros before loading
    for (int i = 0; i < 26; i++) {
        if (state->macro_registers[i]) {
            free(state->macro_registers[i]);
            state->macro_registers[i] = NULL;
        }
    }

    char line[MAX_LINE_LEN * 4]; // Allow for escaped content
    while (fgets(line, sizeof(line), f)) {
        if (strlen(line) < 3 || line[1] != ':') continue; // Invalid line format

        int reg_idx = line[0] - 'a';
        if (reg_idx < 0 || reg_idx >= 26) continue; // Invalid register

        char *escaped_content = line + 2;
        // Trim newline
        escaped_content[strcspn(escaped_content, "\n")] = 0;

        char *unescaped = malloc(strlen(escaped_content) + 1);
        size_t unescaped_len = 0;
        for (size_t i = 0; i < strlen(escaped_content); ) {
            if (escaped_content[i] == '\\') {
                i++; // Move past the backslash
                if (i >= strlen(escaped_content)) break;
                
                if (escaped_content[i] == '\\') {
                    unescaped[unescaped_len++] = '\\';
                    i++;
                } else if (escaped_content[i] == 'n') {
                    unescaped[unescaped_len++] = '\n';
                    i++;
                } else if (escaped_content[i] == 'x') {
                    if (i + 2 < strlen(escaped_content)) {
                        char hex[3] = { escaped_content[i+1], escaped_content[i+2], '\0' };
                        unescaped[unescaped_len++] = (char)strtol(hex, NULL, 16);
                        i += 3;
                    } else { i++; }
                } else {
                    // Not a valid escape, just treat it literally
                    unescaped[unescaped_len++] = escaped_content[i];
                    i++;
                }
            } else {
                unescaped[unescaped_len++] = escaped_content[i];
                i++;
            }
        }
        unescaped[unescaped_len] = '\0';
        
        state->macro_registers[reg_idx] = strdup(unescaped);
        free(unescaped);
    }

    fclose(f);
    // No status message for automatic loading
}
