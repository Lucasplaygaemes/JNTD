#ifndef COMMAND_EXECUTION_H
#define COMMAND_EXECUTION_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for command_execution.c
void process_command(EditorState *state, bool *should_exit);
void execute_shell_command(EditorState *state);
void compile_file(EditorState *state, char* args);
void run_and_display_command(const char* command, const char* title);
void diff_command(EditorState *state, const char *args);
void add_to_command_history(EditorState *state, const char* command);

void copy_selection_to_clipboard(EditorState *state);
void paste_from_clipboard(EditorState *state);

// LSP related command processing functions
void process_lsp_restart(EditorState *state);
void process_lsp_diagnostics(EditorState *state);
void process_lsp_definition(EditorState *state);
void process_lsp_references(EditorState *state);
void process_lsp_rename(EditorState *state, const char *new_name);
void process_lsp_status(EditorState *state);
void process_lsp_hover(EditorState *state);
void process_lsp_symbols(EditorState *state);

// Utility function
void get_word_at_cursor(EditorState *state, char *buffer, size_t buffer_size);

void executar_comando_no_terminal(const char *comando_str);

#endif // COMMAND_EXECUTION_H
