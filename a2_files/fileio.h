#ifndef FILEIO_H
#define FILEIO_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for fileio.c
void load_file_core(EditorState *state, const char *filename);
void load_file(EditorState *state, const char *filename);
void save_file(EditorState *state);
void auto_save(EditorState *state);
time_t get_file_mod_time(const char *filename);
void check_external_modification(EditorState *state);
void editor_reload_file(EditorState *state);
void load_syntax_file(EditorState *state, const char *filename);
void save_last_line(const char *filename, int line);
int load_last_line(const char *filename);
FileRecoveryChoice display_recovery_prompt(WINDOW *parent_win, EditorState *state);
void handle_file_recovery(EditorState *state, const char *original_filename, const char *sv_filename);
void save_macros(EditorState *state);
void load_macros(EditorState *state);

// Helper function
const char * get_syntax_file_from_extension(const char* filename);

#endif // FILEIO_H