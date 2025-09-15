#ifndef LSP_CLIENT_H
#define LSP_CLIENT_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for lsp_client.c
LspMessage* lsp_create_message();
void lsp_free_message(LspMessage *msg);
LspMessage* lsp_parse_message(const char *json_str);
char* lsp_serialize_message(LspMessage *msg);
void lsp_parse_diagnostics(EditorState *state, const char *json_response);
void lsp_did_change(EditorState *state);
void lsp_did_save(EditorState *state);
void lsp_shutdown(EditorState *state);
void lsp_did_open(EditorState *state);
char* json_escape_string(const char *str);
void lsp_request_diagnostics(EditorState *state);
void lsp_force_diagnostics(EditorState *state);
LspDiagnostic* get_diagnostic_under_cursor(EditorState *state); // This is used in screen_ui.c, so it needs to be public.
void lsp_initialize(EditorState *state);
bool lsp_is_available(EditorState *state);
char* lsp_get_uri_from_path(const char *path);
void lsp_init_document_state(EditorState *state);
void lsp_free_document_state(EditorState *state);
void lsp_cleanup_diagnostics(EditorState *state);
void lsp_parse_completion(EditorState *state, const char *json_response);
void lsp_send_initialize(EditorState *state);
void lsp_send_message(EditorState *state, const char *json_message);
void lsp_send_did_change(EditorState *state);
bool lsp_process_alive(EditorState *state);
bool lsp_is_ready(EditorState *state);
void lsp_process_messages(EditorState *state);
void lsp_process_received_data(EditorState *state, const char *buffer, size_t buffer_len);
void lsp_log(const char *format, ...);
void lsp_draw_diagnostics(WINDOW *win, EditorState *state);

#endif // LSP_CLIENT_H