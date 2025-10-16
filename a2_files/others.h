#ifndef OTHERS_H
#define OTHERS_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for others.c
// Bracket Matching
void editor_find_unmatched_brackets(EditorState *state);
bool is_unmatched_bracket(EditorState *state, int line, int col);

// Text Editing & Manipulation
void editor_yank_selection(EditorState *state);
void editor_global_yank(EditorState *state);
void editor_paste(EditorState *state);
void editor_global_paste(EditorState *state);
void editor_handle_enter(EditorState *state);
void editor_handle_backspace(EditorState *state);
void editor_insert_char(EditorState *state, wint_t ch);
void editor_delete_line(EditorState *state);
void editor_delete_selection(EditorState *state);
void editor_yank_line(EditorState *state);
void editor_yank_to_move_register(EditorState *state);
void editor_paste_from_move_register(EditorState *state);
char* trim_whitespace(char *str);
void editor_ident_line(EditorState *state, int line_num);
void editor_unindent_line(EditorState *state, int line_num);

// Cursor Movement & Navigation
void ensure_cursor_in_bounds(EditorState *state);
void editor_move_to_next_word(EditorState *state);
void editor_move_to_previous_word(EditorState *state);

// Search
void editor_find(EditorState *state);
void editor_find_next(EditorState *state);
void editor_find_previous(EditorState *state);

// Undo/Redo
EditorSnapshot* create_snapshot(EditorState *state);
void free_snapshot(EditorSnapshot *snapshot);
void restore_from_snapshot(EditorState *state, EditorSnapshot *snapshot);
void push_undo(EditorState *state);
void clear_redo_stack(EditorState *state);
void do_undo(EditorState *state);
void do_redo(EditorState *state);

// Autocompletion
void add_suggestion(EditorState *state, const char *suggestion);
void editor_start_completion(EditorState *state);
void editor_start_command_completion(EditorState *state);
void editor_start_file_completion(EditorState *state);
void editor_end_completion(EditorState *state);
void editor_apply_completion(EditorState *state);
void editor_draw_completion_win(WINDOW *win, EditorState *state);

// Input Handling
void handle_insert_mode_key(EditorState *state, wint_t ch);
void handle_command_mode_key(EditorState *state, wint_t ch, bool *should_exit);

void editor_toggle_comment(EditorState *state);


#endif // OTHERS_H
