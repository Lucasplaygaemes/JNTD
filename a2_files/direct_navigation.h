#ifndef DIRECT_NAVIGATION_H
#define DIRECT_NAVIGATION_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for direct_navigation.c
void get_history_filename(char *buffer, size_t size);
int compare_dirs(const void *a, const void *b); // Helper, might not need to be public
void load_directory_history(EditorState *state);
void save_directory_history(EditorState *state);
void update_directory_access(EditorState *state, const char *path);
void change_directory(EditorState *state, const char *new_path);
void display_directory_navigator(EditorState *state);
void prompt_for_directory_change(EditorState *state);
void load_file_history(EditorState *state);
void save_file_history(EditorState *state);
void add_to_file_history(EditorState *state, const char *path);

#endif // DIRECT_NAVIGATION_H
