#ifndef SCREEN_UI_H
#define SCREEN_UI_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for screen_ui.c
void draw_diagnostic_popup(WINDOW *main_win, EditorState *state, const char *message);
void editor_redraw(WINDOW *win, EditorState *state);
void adjust_viewport(WINDOW *win, EditorState *state);
void get_visual_pos(WINDOW *win, EditorState *state, int *visual_y, int *visual_x);
int get_visual_col(const char *line, int byte_col);
void display_help_screen();
void display_output_screen(const char *title, const char *filename);
FileViewer* create_file_viewer(const char* filename);
void destroy_file_viewer(FileViewer* viewer);

#endif // SCREEN_UI_H