#include "command_execution.h" // Include its own header
#include "defs.h" // For EditorState, etc.
#include "fileio.h" // For save_file, load_file, get_syntax_file_from_extension, load_syntax_file
#include "lsp_client.h" // For lsp_did_save, lsp_did_change
#include "screen_ui.h" // For display_help_screen, display_output_screen
#include "window_managment.h" // For fechar_janela_ativa
#include "others.h" // For trim_whitespace, display_work_summary
#include "timer.h" // For display_work_summary

#include <ctype.h> // For isspace
#include <errno.h> // For errno
#include <unistd.h> // For chdir, getcwd, close
#include <sys/wait.h> // For WIFEXITED, WEXITSTATUS

// ===================================================================
// 6. Command Execution & Processing
// ===================================================================

void process_command(EditorState *state, bool *should_exit) {
    if (state->command_buffer[0] == '!') {
        execute_shell_command(state);
        add_to_command_history(state, state->command_buffer);
        state->mode = NORMAL;
        return;
    }
    add_to_command_history(state, state->command_buffer);
    char command[100], args[1024] = "";
    char *buffer_ptr = state->command_buffer;
    int i = 0;
    while(i < 99 && *buffer_ptr && !isspace(*buffer_ptr)) command[i++] = *buffer_ptr++;
    command[i] = '\0';
    if(isspace(*buffer_ptr)) buffer_ptr++;
    char *trimmed_args = trim_whitespace(buffer_ptr);
    strncpy(args, trimmed_args, sizeof(args) - 1);
    args[sizeof(args)-1] = '\0';

    if (strcmp(command, "q") == 0) {
        if (state->buffer_modified) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
            state->mode = NORMAL;
            return;
        }
        fechar_janela_ativa(should_exit);
        return; 
    } else if (strcmp(command, "q!") == 0) {
        state->buffer_modified = false;
        fechar_janela_ativa(should_exit);
        return;
    } else if (strcmp(command, "wq") == 0) {
        save_file(state);
        if (state->lsp_enabled) {
            lsp_did_save(state);
        }
        *should_exit = true;
        fechar_janela_ativa(should_exit);
        return;
    } else if (strcmp(command, "w") == 0) {
        if (strlen(args) > 0) {
            strncpy(state->filename, args, sizeof(state->filename) - 1);
            const char * syntax_file =  get_syntax_file_from_extension(args);
            load_syntax_file(state, syntax_file);
            }
        save_file(state);
        if (state->lsp_enabled) {
            lsp_did_save(state);
            }
    } else if (strcmp(command, "help") == 0) {
        display_help_screen();
    } else if (strcmp(command, "gcc") == 0) {
        compile_file(state, args);
    } else if (strcmp(command, "rc") == 0) {
        editor_reload_file(state);
    } else if (strcmp(command, "rc!") == 0) {
        if (strcmp(state->filename, "[No Name]") == 0) {
            snprintf(state->status_msg, sizeof(state->status_msg), "No file name to reload.");
        } else {
            load_file(state, state->filename);
            snprintf(state->status_msg, sizeof(state->status_msg), "File reloaded (force).");
        }
    } else if (strcmp(command, "open") == 0) {
        if (strlen(args) > 0) {
            load_file(state, args);
            lsp_initialize(state); // Initialize LSP for the new file
        } else {
            snprintf(state->status_msg, sizeof(state->status_msg), "Usage: :open <filename>");
        }
    } else if (strcmp(command, "new") == 0) {
        for (int i = 0; i < state->num_lines; i++) { if(state->lines[i]) {
        free(state->lines[i]); state->lines[i] = NULL; } }
        state->num_lines = 1; state->lines[0] = calloc(1, 1); strcpy(state->filename, "[No Name]");
        state->current_line = 0; state->current_col = 0; state->ideal_col = 0; state->top_line = 0; state->left_col = 0;
        snprintf(state->status_msg, sizeof(state->status_msg), "New file opened.");
    } else if (strcmp(command, "timer") == 0) {
        def_prog_mode(); endwin();
        display_work_summary();
        reset_prog_mode(); refresh();
    } else if (strcmp(command, "diff") == 0) {
        diff_command(state, args);
    } else if (strcmp(command, "set") == 0) {
        if (strcmp(args, "paste") == 0) {
            state->paste_mode = true;
            state->auto_indent_on_newline = false;
            snprintf(state->status_msg, sizeof(state->status_msg), "-- PASTE MODE ON --");
        } else if (strcmp(args, "nopaste") == 0) {
            state->paste_mode = false;
            state->auto_indent_on_newline = true;
            snprintf(state->status_msg, sizeof(state->status_msg), "-- PASTE MODE OFF --");
        }
        else if (strcmp(args, "wrap") == 0) {
            state->word_wrap_enabled = true;
            snprintf(state->status_msg, sizeof(state->status_msg), "Word wrap ativado");
        } else if (strcmp(args, "nowrap") == 0) {
            state->word_wrap_enabled = false;
            snprintf(state->status_msg, sizeof(state->status_msg), "Word wrap desativado");
        }
        else {
            snprintf(state->status_msg, sizeof(state->status_msg), "Argumento desconhecido para set: %s", args);
        }
    
      // Comandos LSP
    } else if (strncmp(command, "lsp-restart", 11) == 0) {
          process_lsp_restart(state);
      
    } else if (strncmp(command, "lsp-diag", 8) == 0) {
          process_lsp_diagnostics(state);
    
    } else if (strncmp(command, "lsp-definition", 14) == 0) {
          process_lsp_definition(state);
    
    } else if (strncmp(command, "lsp-references", 14) == 0) {
          process_lsp_references(state);
      
    } else if (strncmp(command, "lsp-rename", 10) == 0) {
          // Extrair o novo nome do comando (formato: lsp-rename novo_nome)
          char *space = strchr(command, ' ');
          if (space) {
              process_lsp_rename(state, space + 1);
          } else {
              snprintf(state->status_msg, STATUS_MSG_LEN, "Uso: lsp-rename <novo_nome>");
          }
    } else if (strcmp(command, "lsp-status") == 0) {
          process_lsp_status(state);
          
    } else if (strcmp(command, "lsp-hover") == 0) {
          process_lsp_hover(state);
          
    } else if (strcmp(command, "lsp-symbols") == 0) {
          process_lsp_symbols(state);
    } else if (strcmp(command, "lsp-refresh") == 0) {
        if (state->lsp_enabled) {
            lsp_did_change(state);
            snprintf(state->status_msg, STATUS_MSG_LEN, "Diagnósticos atualizados");
        } else {
            snprintf(state->status_msg, STATUS_MSG_LEN, "LSP não está ativo");
        }
    } else if (strcmp(command, "lsp-check") == 0) {
        if (state->lsp_enabled) {
            // Forçar uma mudança para trigger diagnósticos
            lsp_did_change(state);
            snprintf(state->status_msg, STATUS_MSG_LEN, "Verificação LSP forçada");
        } else {
            snprintf(state->status_msg, STATUS_MSG_LEN, "LSP não está ativo");
        }
    } else if (strcmp(command, "lsp-debug") == 0) {
        if (state->lsp_enabled) {
              // Forçar o envio de didChange para trigger diagnósticos
              lsp_did_change(state);
              snprintf(state->status_msg, STATUS_MSG_LEN, "Debug LSP: didChange enviado");
          }
         else {
              snprintf(state->status_msg, STATUS_MSG_LEN, "LSP não está ativo");
          }
    } else if (strcmp(command, "toggle_auto_indent") == 0) {
        state->auto_indent_on_newline = !state->auto_indent_on_newline;
        snprintf(state->status_msg, sizeof(state->status_msg), "Auto-indent on newline: %s", state->auto_indent_on_newline ? "ON" : "OFF");
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Unknown command: %s", command);
    }
    state->mode = NORMAL;
}

void execute_shell_command(EditorState *state) {
    char *cmd = state->command_buffer + 1;
    if (strncmp(cmd, "cd ", 3) == 0) {
        char *path = cmd + 3;
        if (chdir(path) != 0) {
            snprintf(state->status_msg, sizeof(state->status_msg), "Erro ao mudar diretório: %s", strerror(errno));
        } else {
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd)) != 0) {
                char display_cwd[80];
                strncpy(display_cwd, cwd, sizeof(display_cwd) - 1);
                display_cwd[sizeof(display_cwd) - 1] = '\0';
                snprintf(state->status_msg, sizeof(state->status_msg), "Diretório atual: %s", display_cwd);
            }
        }
        return;
    }
    char temp_output_file[] = "/tmp/editor_shell_output.XXXXXX";
    int fd = mkstemp(temp_output_file);
    if(fd == -1) { snprintf(state->status_msg, sizeof(state->status_msg), "Erro ao criar arquivo temporário."); return; }
    close(fd);
    char full_shell_command[2048];
    snprintf(full_shell_command, sizeof(full_shell_command), "%s > %s 2>&1", cmd, temp_output_file);
    def_prog_mode(); endwin();
    system(full_shell_command);
    reset_prog_mode(); refresh();
    FILE *f = fopen(temp_output_file, "r");
    if(f) {
        fseek(f, 0, SEEK_END); long size = ftell(f); fclose(f);
        long max_status_bar_size = 70;
        if (size > 0 && size <= max_status_bar_size) {
            FILE *read_f = fopen(temp_output_file, "r");
            char buffer[max_status_bar_size + 2];
            size_t n = fread(buffer, 1, sizeof(buffer) - 1, read_f);
            fclose(read_f);
            buffer[n] = '\0';

            if (n > 0 && buffer[n-1] == '\n') {
                buffer[n-1] = '\0';
            }

            if(strchr(buffer, '\n') == NULL) {
                snprintf(state->status_msg, sizeof(state->status_msg), "Saída: %s", buffer);
                remove(temp_output_file);
                return;
            }
        }
        display_output_screen("---\
SAÍDA DO COMANDO---", temp_output_file);
        snprintf(state->status_msg, sizeof(state->status_msg), "Comando '%s' executado.", cmd);
    } else {
        snprintf(state->status_msg, sizeof(state->status_msg), "Comando executado, mas sem saída.");
        remove(temp_output_file);
    }
}

void compile_file(EditorState *state, char* args) {
    int ret; // Declared here
    save_file(state);
    if (strcmp(state->filename, "[No Name]") == 0) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Salve o arquivo com um nome antes de compilar.");
        return;
    }
    char output_filename[300];
    strncpy(output_filename, state->filename, sizeof(output_filename) - 1);
    char *dot = strrchr(output_filename, '.'); if (dot) *dot = '\0';
    char command[1024];
    snprintf(command, sizeof(command), "gcc %s -o %s %s", state->filename, output_filename, args);
    char temp_output_file[] = "/tmp/editor_compile_output.XXXXXX";
    int fd = mkstemp(temp_output_file);
    if(fd == -1) { snprintf(state->status_msg, sizeof(state->status_msg), "Erro ao criar arquivo temporário."); return; }
    close(fd);
    char full_shell_command[2048];
    snprintf(full_shell_command, sizeof(full_shell_command), "%s > %s 2>&1", command, temp_output_file);
    def_prog_mode(); endwin();
    ret = system(full_shell_command);
    reset_prog_mode(); refresh();
    if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        char display_output_name[40];
        strncpy(display_output_name, output_filename, sizeof(display_output_name) - 1);
        display_output_name[sizeof(display_output_name)-1] = '\0';
        snprintf(state->status_msg, sizeof(state->status_msg), "Compilação bem-sucedida! Executável: %s", display_output_name);
    } else {
        display_output_screen("---ERROS DE COMPILAÇÃO---", temp_output_file);
        snprintf(state->status_msg, sizeof(state->status_msg), "Compilação falhou. Veja os erros.");
    }
}

void run_and_display_command(const char* command, const char* title) {
    char temp_output_file[] = "/tmp/editor_cmd_output.XXXXXX";
    int fd = mkstemp(temp_output_file);
    if (fd == -1) return;
    close(fd);

    char full_shell_command[2048];
    snprintf(full_shell_command, sizeof(full_shell_command), "%s > %s 2>&1", command, temp_output_file);

    def_prog_mode(); endwin();
    system(full_shell_command);
    reset_prog_mode(); refresh();
    
    display_output_screen(title, temp_output_file);
}

void diff_command(EditorState *state, const char *args) {
    char filename1[256] = {0}, filename2[256] = {0};
    if (sscanf(args, "%255s %255s", filename1, filename2) != 2) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Uso: :diff <arquivo1> <arquivo2>");
        return;
    }
    char diff_cmd_str[1024];
    snprintf(diff_cmd_str, sizeof(diff_cmd_str), "git diff --no-index -- %s %s", filename1, filename2);
    run_and_display_command(diff_cmd_str, "---Diferenças---");
}

void add_to_command_history(EditorState *state, const char* command) {
    if (strlen(command) == 0) return;
    if (state->history_count > 0 && strcmp(state->command_history[state->history_count - 1], command) == 0) return;
    if (state->history_count < MAX_COMMAND_HISTORY) {
        state->command_history[state->history_count++] = strdup(command);
    } else {
        free(state->command_history[0]);
        for (int i = 0; i < MAX_COMMAND_HISTORY - 1; i++) {
            state->command_history[i] = state->command_history[i + 1];
        }
        state->command_history[MAX_COMMAND_HISTORY - 1] = strdup(command);
    }
}
