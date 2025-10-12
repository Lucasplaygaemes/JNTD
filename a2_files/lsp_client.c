#include "lsp_client.h"
#include "defs.h"
#include "others.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <jansson.h>
#include <ctype.h>
#include <strings.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>

// Forward declaration to resolve implicit declaration warning
void get_word_at_cursor(EditorState *state, char *buffer, size_t buffer_size);

// Function to convert byte column to character column, considering UTF-8
int get_character_col_from_byte(const char *line, int byte_col) {
    if (!line) return 0;
    int char_col = 0;
    for (int i = 0; i < byte_col && line[i] != '\0'; i++) {
        if ((line[i] & 0xC0) != 0x80) {
            char_col++;
        }
    }
    return char_col;
}

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

// ===================================================================
// Robust implementation of the LSP Client with full JSON parsing
// ===================================================================

// Creates an LSP message
LspMessage* lsp_create_message() {
    LspMessage *msg = malloc(sizeof(LspMessage));
    msg->jsonrpc = strdup("2.0");
    msg->id = NULL;
    msg->method = NULL;
    msg->params = NULL;
    msg->result = NULL;
    msg->error = NULL;
    return msg;
}

// Frees an LSP message
void lsp_free_message(LspMessage *msg) {
    if (!msg) return;
    
    free(msg->jsonrpc);
    if (msg->id) free(msg->id);
    if (msg->method) free(msg->method);
    if (msg->params) json_decref(msg->params);
    if (msg->result) json_decref(msg->result);
    if (msg->error) json_decref(msg->error);
    
    free(msg);
}

LspMessage* lsp_parse_message(const char *json_str) {
    json_error_t error;
    json_t *root = json_loads(json_str, 0, &error);
    
    if (!root) {
        fprintf(stderr, "JSON parse error: %s\n", error.text);
        return NULL;
    }
    
    LspMessage *msg = lsp_create_message();
    
    // Parse jsonrpc
    json_t *jsonrpc = json_object_get(root, "jsonrpc");
    if (jsonrpc && json_is_string(jsonrpc)) {
        free(msg->jsonrpc);
        msg->jsonrpc = strdup(json_string_value(jsonrpc));
    }
    
    // Parse id
    json_t *id = json_object_get(root, "id");
    if (id) {
        if (json_is_string(id)) {
            msg->id = strdup(json_string_value(id));
        } else if (json_is_integer(id)) {
            char id_buf[32];
            snprintf(id_buf, sizeof(id_buf), "%lld", (long long)json_integer_value(id));
            msg->id = strdup(id_buf);
        }
    }
    
    // Parse method
    json_t *method = json_object_get(root, "method");
    if (method && json_is_string(method)) {
        msg->method = strdup(json_string_value(method));
    }
    
    // Parse params
    msg->params = json_object_get(root, "params");
    if (msg->params) json_incref(msg->params);
    
    // Parse result
    msg->result = json_object_get(root, "result");
    if (msg->result) json_incref(msg->result);
    
    // Parse error
    msg->error = json_object_get(root, "error");
    if (msg->error) json_incref(msg->error);
    
    json_decref(root);
    return msg;
}

// Serializes an LspMessage to JSON
char* lsp_serialize_message(LspMessage *msg) {
    json_t *root = json_object();
    
    // Add jsonrpc
    json_object_set_new(root, "jsonrpc", json_string(msg->jsonrpc));
    
    // Add id
    if (msg->id) {
        // Try to parse as integer first
        char *endptr;
        long long id_num = strtoll(msg->id, &endptr, 10);
        if (*endptr == '\0') {
            json_object_set_new(root, "id", json_integer(id_num));
        } else {
            json_object_set_new(root, "id", json_string(msg->id));
        }
    }
    
    // Add method
    if (msg->method) {
        json_object_set_new(root, "method", json_string(msg->method));
    }
    
    // Add params - using json_object_set to not transfer ownership
    if (msg->params) {
        json_object_set(root, "params", msg->params);
    }
    
    // Add result - using json_object_set to not transfer ownership
    if (msg->result) {
        json_object_set(root, "result", msg->result);
    }
    
    // Add error - using json_object_set to not transfer ownership
    if (msg->error) {
        json_object_set(root, "error", msg->error);
    }
    
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    return json_str;
}

// Parses LSP diagnostics
void lsp_parse_diagnostics(EditorState *state, const char *json_response) {
    lsp_log("=== START DIAGNOSTICS PARSING ===\n");
    lsp_log("JSON received: %s\n", json_response);
    
    LspMessage *msg = lsp_parse_message(json_response);
    if (!msg) {
        lsp_log("Failed to parse JSON message\n");
        return;
    }
    
    if (!msg->params) {
        lsp_log("No params in message\n");
        lsp_free_message(msg);
        return;
    }
    
    // Detailed log of parameters
    char *params_str = json_dumps(msg->params, JSON_INDENT(2));
    lsp_log("Params: %s\n", params_str);
    free(params_str);
    lsp_log("Parsing diagnostics: %s\n", json_response);
    lsp_log("Debug: Received diagnostic message\n");
    lsp_log("Drawing %d diagnostics\n", state->lsp_document->diagnostics_count);
    if (!msg || !msg->params) {
        lsp_free_message(msg);
        return;
    }
    lsp_log("Debug: Received diagnostic message\n");
    // Clean up existing diagnostics
    lsp_cleanup_diagnostics(state);
        
    static int diagnostic_counter = 0;
    if (state->lsp_enabled) {
        if (diagnostic_counter++ % 100 == 0) {
            lsp_request_diagnostics(state);
            }
    }
    
    // Get diagnostics array from params
    json_t *diagnostics = json_object_get(msg->params, "diagnostics");
    if (!diagnostics || !json_is_array(diagnostics)) {
        lsp_free_message(msg);
        return;
    }
    
    size_t num_diagnostics = json_array_size(diagnostics);
    if (num_diagnostics > 0) {
        state->lsp_document->diagnostics = malloc(num_diagnostics * sizeof(LspDiagnostic));
        state->lsp_document->diagnostics_count = num_diagnostics;
        
        for (size_t i = 0; i < num_diagnostics; i++) {
            json_t *diag_obj = json_array_get(diagnostics, i);
            LspDiagnostic *diag = &state->lsp_document->diagnostics[i];
            
            // Initialize with defaults
            diag->range.start.line = 0;
            diag->range.start.character = 0;
            diag->range.end.line = 0;
            diag->range.end.character = 0;
            diag->severity = LSP_SEVERITY_ERROR;
            diag->message = NULL;
            diag->code = NULL;
            
            // Parse range
            json_t *range = json_object_get(diag_obj, "range");
            if (range) {
                json_t *start = json_object_get(range, "start");
                if (start) {
                    json_t *line = json_object_get(start, "line");
                    json_t *character = json_object_get(start, "character");
                    if (json_is_integer(line)) diag->range.start.line = json_integer_value(line);
                    if (json_is_integer(character)) diag->range.start.character = json_integer_value(character);
                }
                
                json_t *end = json_object_get(range, "end");
                if (end) {
                    json_t *line = json_object_get(end, "line");
                    json_t *character = json_object_get(end, "character");
                    if (json_is_integer(line)) diag->range.end.line = json_integer_value(line);
                    if (json_is_integer(character)) diag->range.end.character = json_integer_value(character);
                }
            }
            
            // Parse severity
            json_t *severity = json_object_get(diag_obj, "severity");
            if (json_is_integer(severity)) {
                diag->severity = json_integer_value(severity);
            }
            
            // Parse message
            json_t *message = json_object_get(diag_obj, "message");
            if (json_is_string(message)) {
                diag->message = strdup(json_string_value(message));
            } else {
                diag->message = strdup("Unknown error");
            }
            
            // Parse code
            json_t *code = json_object_get(diag_obj, "code");
            if (code) {
                if (json_is_string(code)) {
                    diag->code = strdup(json_string_value(code));
                } else if (json_is_integer(code)) {
                    char code_buf[32];
                    snprintf(code_buf, sizeof(code_buf), "%lld", (long long)json_integer_value(code));
                    diag->code = strdup(code_buf);
                } else {
                    diag->code = strdup("unknown");
                }
            }
            else {
                diag->code = strdup("unknown");
            }
        }
    }
    
    lsp_free_message(msg);
    lsp_log("Debug: %d diagnostics processed\n", state->lsp_document->diagnostics_count);
}

void process_lsp_status(EditorState *state) {
    if (state->lsp_enabled && state->lsp_client) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP active for %s (PID: %d)", 
                state->lsp_client->languageId, state->lsp_client->server_pid);
    } else {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP not active");
    }
}

void process_lsp_hover(EditorState *state) {
    if (!lsp_is_available(state)) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP not available");
        return;
    }
    
    char current_word[100];
    get_word_at_cursor(state, current_word, sizeof(current_word));
    
    if (current_word[0] != '\0') {
        // Basic simulation - in practice, it would send a hover request to the LSP
        snprintf(state->status_msg, STATUS_MSG_LEN, "Information about: %s", current_word);
    } else {
        snprintf(state->status_msg, STATUS_MSG_LEN, "No symbol under cursor");
    }
}

void process_lsp_symbols(EditorState *state) {
    if (!lsp_is_available(state)) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP not available");
        return;
    }
    
    // Basic simulation - count functions and variables
    int function_count = 0;
    int variable_count = 0;
    
    for (int i = 0; i < state->num_lines; i++) {
        if (state->lines[i]) {
            // Search for functions
            if (strstr(state->lines[i], "(") && strstr(state->lines[i], ")")) {
                if (strstr(state->lines[i], "void") || strstr(state->lines[i], "int") ||
                    strstr(state->lines[i], "char") || strstr(state->lines[i], "float") ||
                    strstr(state->lines[i], "double")) {
                    function_count++;
                }
            }
            // Search for variables (simplified)
            if (strstr(state->lines[i], "=") && 
                (strstr(state->lines[i], "int") || strstr(state->lines[i], "char") ||
                 strstr(state->lines[i], "float") || strstr(state->lines[i], "double"))) {
                variable_count++;
            }
        }
    }
    
    snprintf(state->status_msg, STATUS_MSG_LEN, "Symbols: %d functions, %d variables", 
            function_count, variable_count);
}



void lsp_did_change(EditorState *state) {
    if (!lsp_is_available(state)) return;

    // Immediately clears old diagnostics so the UI is updated.
    // The new diagnostics will come from the LSP server.
    lsp_cleanup_diagnostics(state);
    
    state->lsp_document->needs_update = true;
    state->lsp_document->version++;
    
    // Sends the change notification immediately to get quick feedback.
    lsp_send_did_change(state);
}


void lsp_did_save(EditorState *state) {
    if (!lsp_is_available(state)) return;
    
    char *uri = lsp_get_uri_from_path(state->filename);
    if (!uri) return;
    
    char *save_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didSave\",\"params\":{\"textDocument\":{\"uri\":\"%s\"}}}";
    char save_buf[1024];
    snprintf(save_buf, sizeof(save_buf), save_msg, uri);
    
    write(state->lsp_client->stdin_fd, save_buf, strlen(save_buf));
    
    free(uri); // Free the memory allocated for the URI
}


void lsp_shutdown(EditorState *state) {
    if (!state || !state->lsp_client) return;
    static bool shutting_down = false;
    if (shutting_down) return;
    shutting_down = true;
    
    // Check if the lsp_client pointer seems valid (not a low address)
    if ((uintptr_t)state->lsp_client < 0x1000) {
        state->lsp_client = NULL;
        return;
    }
    // Send shutdown and exit messages if the pipes are still valid
    if (state->lsp_client->stdin_fd != -1) {
        char *shutdown_msg = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"shutdown\",\"params\":{}}";
        write(state->lsp_client->stdin_fd, shutdown_msg, strlen(shutdown_msg));
        
        char *exit_msg = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\",\"params\":{}}";
        write(state->lsp_client->stdin_fd, exit_msg, strlen(exit_msg));
    }
    
    // Close pipes
    if (state->lsp_client->stdin_fd != -1) {
        close(state->lsp_client->stdin_fd);
        state->lsp_client->stdin_fd = -1;
    }
    if (state->lsp_client->stdout_fd != -1) {
        close(state->lsp_client->stdout_fd);
        state->lsp_client->stdout_fd = -1;
    }
    if (state->lsp_client->stderr_fd != -1) {
        close(state->lsp_client->stderr_fd);
        state->lsp_client->stderr_fd = -1;
    }
    
    // Wait for the LSP server process
    if (state->lsp_client->server_pid != -1) {
        waitpid(state->lsp_client->server_pid, NULL, 0);
        state->lsp_client->server_pid = -1;
    }
    
    // Free all memory allocated for the LSP client
    if (state->lsp_client->languageId) {
        free(state->lsp_client->languageId);
        state->lsp_client->languageId = NULL;
    }
    if (state->lsp_client->rootUri) {
        free(state->lsp_client->rootUri);
        state->lsp_client->rootUri = NULL;
    }
    if (state->lsp_client->workspaceFolders) {
        free(state->lsp_client->workspaceFolders);
        state->lsp_client->workspaceFolders = NULL;
    }
    if (state->lsp_client->compilerFlags) {
        free(state->lsp_client->compilerFlags);
        state->lsp_client->compilerFlags = NULL;
    }
    if (state->lsp_client->compilationDatabase) {
        free(state->lsp_client->compilationDatabase);
        state->lsp_client->compilationDatabase = NULL;
    }
    
    // Free the main LSP client structure
    free(state->lsp_client);
    state->lsp_client = NULL;
    state->lsp_enabled = false;
    
    // Free LSP document state
    lsp_free_document_state(state);
}

void lsp_did_open(EditorState *state) {
    if (!lsp_is_available(state)) return;
    
    lsp_log("Sending didOpen for: %s\n", state->filename);
    
    // Build the file content
    size_t total_length = 0;
    for (int i = 0; i < state->num_lines; i++) {
        if (state->lines[i]) {
            total_length += strlen(state->lines[i]) + 1; // +1 for newline
        }
    }
    
    char *content = malloc(total_length + 1);
    if (!content) return;
    
    content[0] = '\0';
    for (int i = 0; i < state->num_lines; i++) {
        if (state->lines[i]) {
            strcat(content, state->lines[i]);
            strcat(content, "\n");
        }
    }
    
    // Escape content for JSON
    char *escaped_content = json_escape_string(content);
    free(content);
    
    if (!escaped_content) {
        lsp_log("Failed to escape content for didOpen\n");
        return;
    }
    
    // Create didOpen message
    char *uri = lsp_get_uri_from_path(state->filename);
    char *open_msg_format = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didOpen\",\"params\":{\"textDocument\":{\"uri\":\"%s\",\"languageId\":\"%s\",\"version\":1,\"text\":\"%s\"}}}";
    
    size_t needed_size = snprintf(NULL, 0, open_msg_format, uri, state->lsp_client->languageId, escaped_content) + 1;
    char *open_buf = malloc(needed_size);
    if (!open_buf) {
        lsp_log("Failed to allocate buffer for didOpen\n");
        free(uri);
        free(escaped_content);
        return;
    }
    snprintf(open_buf, needed_size, open_msg_format, uri, state->lsp_client->languageId, escaped_content);
    
    lsp_log("didOpen message: %s\n", open_buf);
    
    // Send the message
    lsp_send_message(state, open_buf);
    
    free(open_buf);
    free(uri);
    free(escaped_content);
    
    // Initialize the document state if it doesn't exist
    if (!state->lsp_document) {
        lsp_init_document_state(state);
    }
    state->lsp_document->needs_update = false;
}

char* json_escape_string(const char *str) {
    if (!str) return strdup("");
    
    // Calculate the required size for the escaped string
    size_t len = strlen(str);
    size_t escaped_len = 0;
    
    // First pass: calculate the required size
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"': case '\\': case '\b': case '\f': 
            case '\n': case '\r': case '\t':
                escaped_len += 2; // \ + special character
                break;
            default:
                if (str[i] < 0x20 || str[i] > 0x7E) {
                    escaped_len += 6; // \uXXXX
                } else {
                    escaped_len += 1;
                }
                break;
        }
    }
    
    // Allocate memory for the escaped string
    char *escaped = malloc(escaped_len + 1);
    if (!escaped) return NULL;
    
    // Second pass: do the escaping
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        switch (str[i]) {
            case '"': 
                escaped[j++] = '\\'; escaped[j++] = '"'; 
                break;
            case '\\': 
                escaped[j++] = '\\'; escaped[j++] = '\\'; 
                break;
            case '\b': 
                escaped[j++] = '\\'; escaped[j++] = 'b'; 
                break;
            case '\f': 
                escaped[j++] = '\\'; escaped[j++] = 'f'; 
                break;
            case '\n': 
                escaped[j++] = '\\'; escaped[j++] = 'n'; 
                break;
            case '\r': 
                escaped[j++] = '\\'; escaped[j++] = 'r'; 
                break;
            case '\t': 
                escaped[j++] = '\\'; escaped[j++] = 't'; 
                break;
            default:
                if (str[i] < 0x20 || str[i] > 0x7E) {
                    // Non-ASCII character: use Unicode escape
                    snprintf(escaped + j, 7, "\\u%04x", (unsigned char)str[i]);
                    j += 6;
                } else {
                    escaped[j++] = str[i];
                }
                break;
        }
    }
    escaped[j] = '\0';
    
    return escaped;
}

void lsp_request_diagnostics(EditorState *state) {
    if (!lsp_is_available(state)) return;
    
    // clangd sends diagnostics automatically after didChange
    // We don't need to request them explicitly
    lsp_log("Diagnostics will be sent automatically by clangd\n");
}

void lsp_force_diagnostics(EditorState *state) {
    if (!lsp_is_available(state)) return;
    
    // Just sends didChange, clangd will automatically respond with diagnostics
    lsp_send_did_change(state);
    lsp_log("Forcing diagnostics update via didChange\n");
}

LspDiagnostic* get_diagnostic_under_cursor(EditorState *state) {
    if (!state->lsp_document || state->lsp_document->diagnostics_count == 0) {
        return NULL;
    }

    // Converts the cursor column from bytes to characters before comparing
    int cursor_char_col = get_character_col_from_byte(state->lines[state->current_line], state->current_col);

    for (int i = 0; i < state->lsp_document->diagnostics_count; i++) {
        LspDiagnostic *diag = &state->lsp_document->diagnostics[i];
        if (state->current_line == diag->range.start.line && 
            cursor_char_col >= diag->range.start.character && 
            cursor_char_col <= diag->range.end.character) {
            return diag;
        }
    }
    return NULL;
}

void process_lsp_restart(EditorState *state) {
    if (state->lsp_client) {
        lsp_shutdown(state);
    }
    if (state->lsp_enabled) {
        napms(500);
        lsp_did_open(state);
    }
    if (state->lsp_enabled) {
        lsp_did_open(state);
    }
    snprintf(state->status_msg, STATUS_MSG_LEN, "LSP restarted");
}

void process_lsp_diagnostics(EditorState *state) {
    if (!lsp_is_available(state)) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP not available");
        return;
    }
    
    if (state->lsp_document && state->lsp_document->diagnostics_count > 0) {
        // Show the first diagnostic as an example
        LspDiagnostic *diag = &state->lsp_document->diagnostics[0];
        snprintf(state->status_msg, STATUS_MSG_LEN, "Diagnostic: %s (Line %d)", 
                diag->message, diag->range.start.line + 1);
    } else {
        snprintf(state->status_msg, STATUS_MSG_LEN, "No diagnostics found");
    }
}

void process_lsp_definition(EditorState *state) {
    if (!lsp_is_available(state)) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP not available");
        return;
    }
    
    // Simplified implementation - in practice, it would send a request to the LSP server
    snprintf(state->status_msg, STATUS_MSG_LEN, "Go to definition - not fully implemented");
    
    // Get the word under the cursor
    char current_word[100];
    get_word_at_cursor(state, current_word, sizeof(current_word));
    
    // Search for the definition in the file itself (simplified implementation)
    for (int i = 0; i < state->num_lines; i++) {
        if (strstr(state->lines[i], current_word) && 
            (strstr(state->lines[i], "int ") || strstr(state->lines[i], "void ") || 
             strstr(state->lines[i], "char ") || strstr(state->lines[i], "float "))) {
            state->current_line = i;
            state->current_col = 0;
            snprintf(state->status_msg, STATUS_MSG_LEN, "Definition of %s found on line %d", 
                    current_word, i + 1);
            return;
        }
    }
    
    snprintf(state->status_msg, STATUS_MSG_LEN, "Definition of %s not found", current_word);
}

void process_lsp_references(EditorState *state) {
    if (!lsp_is_available(state)) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP not available");
        return;
    }
    
    // Simplified implementation
    char current_word[100];
    get_word_at_cursor(state, current_word, sizeof(current_word));
    
    int count = 0;
    for (int i = 0; i < state->num_lines; i++) {
        if (strstr(state->lines[i], current_word)) {
            count++;
        }
    }
    
    snprintf(state->status_msg, STATUS_MSG_LEN, "%d references found for %s", count, current_word);
}

void process_lsp_rename(EditorState *state, const char *new_name) {
    if (!lsp_is_available(state)) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP not available");
        return;
    }
    
    // Simplified implementation - rename in the file itself
    char current_word[100];
    get_word_at_cursor(state, current_word, sizeof(current_word));
    
    if (strlen(new_name) == 0) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "Invalid name for rename");
        return;
    }
    
    int count = 0;
    for (int i = 0; i < state->num_lines; i++) {
        if (state->lines[i]) {
            char *pos = state->lines[i];
            while ((pos = strstr(pos, current_word)) != NULL) {
                // Check if it is a whole word (not part of another word)
                if ((pos == state->lines[i] || !isalnum(pos[-1])) && 
                    !isalnum(pos[strlen(current_word)])) {
                    // Replace the word
                    char new_line[MAX_LINE_LEN];
                    strncpy(new_line, state->lines[i], pos - state->lines[i]);
                    new_line[pos - state->lines[i]] = '\0';
                    strcat(new_line, new_name);
                    strcat(new_line, pos + strlen(current_word));
                    
                    free(state->lines[i]);
                    state->lines[i] = strdup(new_line);
                    count++;
                }
                pos += strlen(current_word);
            }
        }
    }
    
    snprintf(state->status_msg, STATUS_MSG_LEN, "Renamed %s to %s (%d occurrences)", 
            current_word, new_name, count);
    state->buffer_modified = true;
}

// Helper function to get the word under the cursor
void get_word_at_cursor(EditorState *state, char *buffer, size_t buffer_size) {
    if (state->current_line < 0 || state->current_line >= state->num_lines) {
        buffer[0] = '\0';
        return;
    }
    
    char *line = state->lines[state->current_line];
    if (!line || state->current_col >= strlen(line)) {
        buffer[0] = '\0';
        return;
    }
    
    // Find the start of the word
    int start = state->current_col;
    while (start > 0 && isalnum(line[start - 1])) {
        start--;
    }
    
    // Find the end of the word
    int end = state->current_col;
    while (end < strlen(line) && isalnum(line[end])) {
        end++;
    }
    
    // Copy the word
    int length = end - start;
    if (length > 0 && length < buffer_size) {
        strncpy(buffer, line + start, length);
        buffer[length] = '\0';
    } else {
        buffer[0] = '\0';
    }
}

void lsp_initialize(EditorState *state) {
    if (state->lsp_client) {
        lsp_shutdown(state);
    }
    state->lsp_init_time = time(NULL);
    state->lsp_init_retries = 0;

    state->lsp_client = malloc(sizeof(LspClient));
    if (!state->lsp_client) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "Allocation error for LSP");
        return;
    }
    memset(state->lsp_client, 0, sizeof(LspClient)); // Initialize with zeros
    
    
    // Determine the language based on the file extension
    const char *ext = strrchr(state->filename, '.');
    if (ext) {
        if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) {
            state->lsp_client->languageId = strdup("c");
        } else if (strcmp(ext, ".cpp") == 0 || strcmp(ext, ".hpp") == 0) {
            state->lsp_client->languageId = strdup("cpp");
        } else if (strcmp(ext, ".py") == 0) {
            state->lsp_client->languageId = strdup("python");
        } else {
            state->lsp_client->languageId = strdup("plaintext");
        }
    } else {
        state->lsp_client->languageId = strdup("plaintext");
    }
    
    // Check if the languageId allocation was successful
    if (!state->lsp_client->languageId) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "Allocation error for languageId");
        free(state->lsp_client);
        state->lsp_client = NULL;
        return;
    }

    // If the language is plaintext, do not start the LSP server.
    if (strcmp(state->lsp_client->languageId, "plaintext") == 0) {
        free(state->lsp_client->languageId);
        free(state->lsp_client);
        state->lsp_client = NULL;
        state->lsp_enabled = false;
        return;
    }
    
    // Start the LSP server (clangd for C/C++)
    int stdin_pipe[2], stdout_pipe[2], stderr_pipe[2];
    
    if (pipe(stdin_pipe) != 0 || pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
        snprintf(state->status_msg, STATUS_MSG_LEN, "Error creating pipes for LSP");
        free(state->lsp_client->languageId);
        free(state->lsp_client);
        state->lsp_client = NULL;
        return;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stderr_pipe[0]);
        
        dup2(stdin_pipe[0], STDIN_FILENO);
        dup2(stdout_pipe[1], STDOUT_FILENO);
        dup2(stderr_pipe[1], STDERR_FILENO);
        
        // Close all other file descriptors
        for (int i = 3; i < 1024; i++) {
            close(i);
        }
        
        // Start the LSP server
        // In lsp_initialize, use optimized arguments:
        
        if (strcmp(state->lsp_client->languageId, "c") == 0 || strcmp(state->lsp_client->languageId, "cpp") == 0) {
            execlp("clangd", "clangd", 
                   "--background-index", 
                   "--log=error",
                   "--pretty",
                   NULL);
        } else if (strcmp(state->lsp_client->languageId, "python") == 0) {
            execlp("pylsp", "pylsp", NULL);
        }
        exit(1);
    } else if (pid > 0) {
        // Parent process
        close(stdin_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);
        
        state->lsp_client->server_pid = pid;
        state->lsp_client->stdin_fd = stdin_pipe[1];
        state->lsp_client->stdout_fd = stdout_pipe[0];
        state->lsp_client->stderr_fd = stderr_pipe[0];
        
        fcntl(state->lsp_client->stdout_fd, F_SETFL, O_NONBLOCK);
        fcntl(state->lsp_client->stderr_fd, F_SETFL, O_NONBLOCK);
        
        lsp_init_document_state(state);
        lsp_send_initialize(state);
        
        snprintf(state->status_msg, STATUS_MSG_LEN, "LSP initialized for %s", state->lsp_client->languageId);
        state->lsp_enabled = true;
        state->lsp_client->initialized = true;
    } else {
        snprintf(state->status_msg, STATUS_MSG_LEN, "Error starting LSP: %s", strerror(errno));
        
        // Close pipes and free resources
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);
        
        free(state->lsp_client->languageId);
        free(state->lsp_client);
        state->lsp_client = NULL;
    }
}

// Checks if LSP is available
bool lsp_is_available(EditorState *state) {
    return state && state->lsp_client && (uintptr_t)state->lsp_client >= 0x1000 && state->lsp_client->initialized;
}


// Converts a file path to a URI in LSP format
char* lsp_get_uri_from_path(const char *path) {
    char absolute_path[PATH_MAX];
    if (realpath(path, absolute_path) == NULL) {
        // If realpath fails, use the original path
        strncpy(absolute_path, path, PATH_MAX - 1);
        absolute_path[PATH_MAX - 1] = '\0';
    }
    
    size_t path_len = strlen(absolute_path);
    char *uri = malloc(path_len * 3 + 8); // Space for encoding + "file://"
    if (!uri) return NULL;
    
    char *ptr = uri;
    strcpy(ptr, "file://");
    ptr += 7;
    
    // Encode special characters
    for (const char *p = absolute_path; *p; p++) {
        if (*p == ' ') {
            strcpy(ptr, "%20");
            ptr += 3;
        } else if (*p == '#') {
            strcpy(ptr, "%23");
            ptr += 3;
        } else if (*p == '%') {
            strcpy(ptr, "%25");
            ptr += 3;
        } else {
            *ptr++ = *p;
        }
    }
    
    *ptr = '\0';
    return uri;
}
// Initializes the LSP document state
void lsp_init_document_state(EditorState *state) {
    if (!state->lsp_document) {
        state->lsp_document = malloc(sizeof(LspDocumentState));
        memset(state->lsp_document, 0, sizeof(LspDocumentState));
    } else {
        // Free previous URI if it exists
        if (state->lsp_document->uri) {
            free(state->lsp_document->uri);
            state->lsp_document->uri = NULL;
        }
    }
    
    state->lsp_document->uri = lsp_get_uri_from_path(state->filename);
    state->lsp_document->version = 1;
    state->lsp_document->diagnostics = NULL;
    state->lsp_document->diagnostics_count = 0;
    state->lsp_document->needs_update = false;
}

void lsp_free_document_state(EditorState *state) {
    if (!state->lsp_document) return;
    
    if (state->lsp_document->uri) {
        free(state->lsp_document->uri);
        state->lsp_document->uri = NULL;
    }
    
    lsp_cleanup_diagnostics(state);
    
    free(state->lsp_document);
    state->lsp_document = NULL;
}

// Clears LSP diagnostics
void lsp_cleanup_diagnostics(EditorState *state) {
    if (!state->lsp_document || !state->lsp_document->diagnostics) return;
    
    for (int i = 0; i < state->lsp_document->diagnostics_count; i++) {
        free(state->lsp_document->diagnostics[i].message);
        free(state->lsp_document->diagnostics[i].code);
    }
    
    free(state->lsp_document->diagnostics);
    state->lsp_document->diagnostics = NULL;
    state->lsp_document->diagnostics_count = 0;
}

// Parses completion suggestions from LSP
void lsp_parse_completion(EditorState *state, const char *json_response) {
    LspMessage *msg = lsp_parse_message(json_response);
    if (!msg || !msg->result) {
        lsp_free_message(msg);
        return;
    }
    
    // Clean up existing completion
    editor_end_completion(state);
    
    // Get completion items from result
    json_t *items = json_object_get(msg->result, "items");
    if (!items || !json_is_array(items)) {
        lsp_free_message(msg);
        return;
    }
    
    // Start completion mode
    state->completion_mode = COMPLETION_TEXT;
    state->num_suggestions = 0;
    state->selected_suggestion = 0;
    
    size_t num_items = json_array_size(items);
    for (size_t i = 0; i < num_items; i++) {
        json_t *item = json_array_get(items, i);
        json_t *label = json_object_get(item, "label");
        
        if (json_is_string(label)) {
            add_suggestion(state, json_string_value(label));
        }
    }
    
    // Create completion window
    int win_height = min(10, state->num_suggestions);
    int win_width = 50; // Wider to show more information
    int start_y = state->current_line - state->top_line + 1;
    int start_x = state->current_col - state->left_col;
    
    if (start_y + win_height > getmaxy(stdscr)) {
        start_y = getmaxy(stdscr) - win_height;
    }
    
    if (start_x + win_width > getmaxx(stdscr)) {
        start_x = getmaxx(stdscr) - win_width;
    }
    
    state->completion_win = newwin(win_height, win_width, start_y, start_x);
    keypad(state->completion_win, TRUE);
    
    lsp_free_message(msg);
}

void lsp_send_initialize(EditorState *state) {
    json_t *params = json_object();
    json_t *capabilities = json_object();
    json_t *workspace = json_object();
    json_t *textDocument = json_object();
    json_t *completion = json_object();
    json_t *completionItem = json_object();
    json_t *publishDiagnostics = json_object();
    json_t *diagnosticProvider = json_object();
    
    // Build capabilities object with more complete support
    json_object_set_new(completionItem, "snippetSupport", json_false());
    json_object_set_new(completion, "completionItem", completionItem);
    
    json_object_set_new(publishDiagnostics, "relatedInformation", json_false());
    json_object_set_new(textDocument, "publishDiagnostics", publishDiagnostics);
    
    // Add diagnostic provider support
    json_object_set_new(diagnosticProvider, "interFileDependencies", json_false());
    json_object_set_new(diagnosticProvider, "workspaceDiagnostics", json_false());
    json_object_set_new(textDocument, "diagnostic", diagnosticProvider);
    
    // Add more text document capabilities
    json_object_set_new(textDocument, "synchronization", json_object());
    json_object_set_new(textDocument, "completion", completion);
    json_object_set_new(textDocument, "hover", json_object());
    json_object_set_new(textDocument, "signatureHelp", json_object());
    json_object_set_new(textDocument, "references", json_object());
    json_object_set_new(textDocument, "definition", json_object());
    json_object_set_new(textDocument, "typeDefinition", json_object());
    json_object_set_new(textDocument, "implementation", json_object());
    json_object_set_new(textDocument, "documentSymbol", json_object());
    
    json_object_set_new(capabilities, "workspace", workspace);
    json_object_set_new(capabilities, "textDocument", textDocument);
    
    // Build params object
    json_object_set_new(params, "processId", json_integer(getpid()));
    
    // Get current working directory for rootUri
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        char *uri = lsp_get_uri_from_path(cwd);
        json_object_set_new(params, "rootUri", json_string(uri));
        free(uri);
    } else {
        json_object_set_new(params, "rootUri", json_string("file:///tmp"));
    }
/*
    // Check for compile_commands.json
    char compile_commands_path[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        snprintf(compile_commands_path, sizeof(compile_commands_path), "%s/compile_commands.json", cwd);
        
        if (access(compile_commands_path, F_OK) == 0) {
            json_object_set_new(params, "compilationDatabase", json_string(compile_commands_path));
        } else {
            json_t *args = json_array();
            const char *default_flags[] = {"-Wall", "-Wextra", "-std=c11", NULL};
            
            for (int i = 0; default_flags[i] != NULL; i++) {
                json_array_append_new(args, json_string(default_flags[i]));
            }
            
            json_object_set_new(params, "compilerArgs", args);
        }
    }
*/
    json_t *initOptions = json_object();
    if (strcmp(state->lsp_client->languageId, "c") == 0 || strcmp(state->lsp_client->languageId, "cpp") == 0) {
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            json_object_set_new(initOptions, "compilationDatabasePath", json_string(cwd));
            }
        } else if (strcmp(state->lsp_client->languageId, "python") == 0) {
            json_t *pylsp_plugins = json_object();
            
            json_t *ruff_plugin = json_object();
            json_object_set_new(ruff_plugin, "enabled", json_true());
            json_object_set_new(pylsp_plugins, "ruff", ruff_plugin);
            
            json_t *pycodestyle_plugin = json_object();
            json_object_set_new(pycodestyle_plugin, "enabled", json_true());
            json_object_set_new(pylsp_plugins, "pycodestyle", pycodestyle_plugin);
            
            json_t *pyflakes_plugin = json_object();
            json_object_set_new(pyflakes_plugin, "enabled", json_true());
            json_object_set_new(pylsp_plugins, "pyflakes", pyflakes_plugin);
                
            json_object_set_new(initOptions, "plugins", pylsp_plugins);
        }
        if (json_object_size(initOptions) > 0) {
            json_object_set_new(params, "initializationOptions", initOptions);
        } else {
            json_decref(initOptions);
     }
            
    // Create and send message
    LspMessage *msg = lsp_create_message();
    msg->id = strdup("1");
    msg->method = strdup("initialize");
    msg->params = params;
    char *json_str = lsp_serialize_message(msg);
    
    FILE *log = fopen("/tmp/lsp_log.txt", "a");
    if (log) {
        fprintf(log, "Sending initialization message:\n%s\n", json_str);
        fclose(log);
    }
    
    // Add Content-Length header as per LSP protocol
    char header[100];
    size_t content_length = strlen(json_str);
    snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", content_length);
    
    // Send header followed by JSON content
    write(state->lsp_client->stdin_fd, header, strlen(header));
    ssize_t bytes_written = write(state->lsp_client->stdin_fd, json_str, content_length);
    
    log = fopen("/tmp/lsp_log.txt", "a");
    if (log) {
        fprintf(log, "Bytes written: %zd (header: %zu, content: %zu)\n", 
                bytes_written, strlen(header), content_length);
        fclose(log);
    }
    lsp_send_message(state, json_str);
    free(json_str);
    lsp_free_message(msg);
}


void lsp_send_message(EditorState *state, const char *json_message) {
    if (!lsp_is_available(state) || !json_message) return;
    
    char header[100];
    size_t content_length = strlen(json_message);
    snprintf(header, sizeof(header), "Content-Length: %zu\r\n\r\n", content_length);
    
    lsp_log("Sending header: %s", header);
    lsp_log("Sending message: %s\n", json_message);
    
    write(state->lsp_client->stdin_fd, header, strlen(header));
    write(state->lsp_client->stdin_fd, json_message, content_length);
}

void lsp_send_did_change(EditorState *state) {
    if (!lsp_is_available(state)) return;
    
    // Build the file content in a simpler way
    size_t total_length = 0;
    for (int i = 0; i < state->num_lines; i++) {
        if (state->lines[i]) {
            total_length += strlen(state->lines[i]) + 1; // +1 for newline
        }
    }
    
    char *content = malloc(total_length + 1);
    if (!content) return;
    
    content[0] = '\0';
    for (int i = 0; i < state->num_lines; i++) {
        if (state->lines[i]) {
            strcat(content, state->lines[i]);
            strcat(content, "\n");
        }
    }
    
    // Escape content for JSON
    char *escaped_content = json_escape_string(content);
    free(content);
    
    if (!escaped_content) {
        lsp_log("Failed to escape content\n");
        return;
    }
    
    // Create didChange message
    char *uri = lsp_get_uri_from_path(state->filename);
    char *change_msg_format = "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/didChange\",\"params\":{\"textDocument\":{\"uri\":\"%s\",\"version\":%d},\"contentChanges\":[{\"text\":\"%s\"}]}}";
    
    size_t needed_size = snprintf(NULL, 0, change_msg_format, uri, state->lsp_document->version, escaped_content) + 1;
    char *change_buf = malloc(needed_size);
    if (!change_buf) {
        lsp_log("Failed to allocate buffer for didChange\n");
        free(uri);
        free(escaped_content);
        return;
    }
    snprintf(change_buf, needed_size, change_msg_format, uri, 
             state->lsp_document->version, escaped_content);
    
    lsp_log("Sending didChange: %s\n", change_buf);
    
    lsp_send_message(state, change_buf);
    
    free(change_buf);
    free(uri);
    free(escaped_content);
    state->lsp_document->version++;
    state->lsp_document->needs_update = false;
}

bool lsp_process_alive(EditorState *state) {
    if (!state->lsp_client) return false;
    
    int status;
    pid_t result = waitpid(state->lsp_client->server_pid, &status, WNOHANG);
    
    if (result == 0) {
        return true; // Process is still running
    } else {
        return false; // Process finished
    }
}


bool lsp_is_ready(EditorState *state) {
    return state->lsp_client != NULL && 
           state->lsp_client->initialized && 
           state->lsp_document != NULL;
}

void lsp_process_messages(EditorState *state) {
    if (!lsp_is_available(state)) return;
    
    fd_set readfds;
    struct timeval tv = {0, 10000}; // 10ms timeout
    
    FD_ZERO(&readfds);
    FD_SET(state->lsp_client->stdout_fd, &readfds);
    
    int retval = select(state->lsp_client->stdout_fd + 1, &readfds, NULL, NULL, &tv);
    if (retval <= 0) return;
    
    char buffer[4096];
    ssize_t bytes_read = read(state->lsp_client->stdout_fd, buffer, sizeof(buffer) - 1);
    
    time_t now = time(NULL);
    if (state->lsp_client && !state->lsp_client->initialized && 
        now - state->lsp_init_time > 5) {
        if (state->lsp_init_retries < 3) {
            lsp_log("LSP initialization timeout, retrying...\n");
            lsp_send_initialize(state);
            state->lsp_init_time = now;
            state->lsp_init_retries++;
        } else {
            lsp_log("LSP initialization failed after multiple retries\n");
            lsp_shutdown(state);
        }
    }
    
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        lsp_log("Raw data received: %s\n", buffer);
        lsp_process_received_data(state, buffer, bytes_read);
    }
}

void lsp_process_received_data(EditorState *state, const char *buffer, size_t buffer_len) {
    static char *message_buffer = NULL;
    static size_t message_pos = 0;
    static size_t buffer_capacity = 0;

    // Ensure the buffer has capacity for the new data
    if (message_pos + buffer_len + 1 > buffer_capacity) {
        size_t new_capacity = buffer_capacity == 0 ? 32768 : buffer_capacity;
        while (new_capacity < message_pos + buffer_len + 1) {
            new_capacity *= 2;
        }
        char *new_buffer = realloc(message_buffer, new_capacity);
        if (!new_buffer) {
            lsp_log("Failed to reallocate LSP message buffer\n");
            free(message_buffer);
            message_buffer = NULL;
            message_pos = 0;
            buffer_capacity = 0;
            return;
        }
        message_buffer = new_buffer;
        buffer_capacity = new_capacity;
    }
    
    // Add new data to the buffer
    memcpy(message_buffer + message_pos, buffer, buffer_len);
    message_pos += buffer_len;
    message_buffer[message_pos] = '\0';
    
    // Process complete messages
    char *ptr = message_buffer;
    while (ptr < message_buffer + message_pos) {
        char *content_length_start = strstr(ptr, "Content-Length:");
        if (!content_length_start) break;
        
        char *header_end = strstr(content_length_start, "\r\n\r\n");
        if (!header_end) break;
        
        long content_length = strtol(content_length_start + 15, NULL, 10);
        if (content_length <= 0) {
             ptr = header_end + 4;
             continue;
        }
        char *json_start = header_end + 4;
        
        if ((size_t)(message_buffer + message_pos - json_start) < (size_t)content_length) break;
        
        // Allocate memory for the JSON message
        char *json_message = malloc(content_length + 1);
        if (!json_message) {
            lsp_log("Failed to allocate buffer for JSON message\n");
            ptr = json_start + content_length; // Skip this message
            continue;
        }
        memcpy(json_message, json_start, content_length);
        json_message[content_length] = '\0';
        
        lsp_log("Processing JSON message: %s\n", json_message);
        
        LspMessage *msg = lsp_parse_message(json_message);
        if (msg) {
            if (msg->method && strstr(msg->method, "textDocument/publishDiagnostics")) {
                lsp_parse_diagnostics(state, json_message);
            } else if (msg->id && strcmp(msg->id, "1") == 0 && msg->result) {
                lsp_log("Initialize response received\n");
                lsp_did_open(state);
            }
            lsp_free_message(msg);
        }
        
        free(json_message); // Free the message memory
        
        ptr = json_start + content_length;
    }
    
    if (ptr > message_buffer) {
        size_t remaining = message_pos - (ptr - message_buffer);
        memmove(message_buffer, ptr, remaining);
        message_pos = remaining;
    }
}

void lsp_log(const char *format, ...) {
    FILE *log_file = fopen("/tmp/editor_lsp.log", "a");
    if (log_file) {
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fclose(log_file);
    }
}

void lsp_draw_diagnostics(WINDOW *win, EditorState *state) {
    if (!state->lsp_document || state->lsp_document->diagnostics_count == 0) {
        return;
    }
    
    for (int i = 0; i < state->lsp_document->diagnostics_count; i++) {
        LspDiagnostic *diag = &state->lsp_document->diagnostics[i];
        
        // Check if the diagnostic is in the visible area
        if (diag->range.start.line >= state->top_line && 
            diag->range.start.line < state->top_line + getmaxy(win) - 2) {
            
            int y = diag->range.start.line - state->top_line;
            int start_x = diag->range.start.character - state->left_col;
            int end_x = diag->range.end.character - state->left_col;
            
            // Adjust coordinates to be inside the visible window
            if (start_x < 0) start_x = 0;
            if (end_x > getmaxx(win)) end_x = getmaxx(win);
            
            if (start_x < end_x && y >= 0 && y < getmaxy(win)) {
                // Choose color based on severity
                int color_pair;
                switch (diag->severity) {
                    case LSP_SEVERITY_ERROR: color_pair = 11; break; // Red
                    case LSP_SEVERITY_WARNING: color_pair = 3; break; // Yellow
                    case LSP_SEVERITY_INFO: color_pair = 6; break; // Cyan
                    case LSP_SEVERITY_HINT: color_pair = 4; break; // Green
                    default: color_pair = 8; break; // White
                }
                
                // Draw brackets and underline the diagnostic range
                wattron(win, COLOR_PAIR(color_pair));
                if (start_x >= 1) {
                    mvwaddch(win, y, start_x - 1, '[');
                }
                wattroff(win, COLOR_PAIR(color_pair));

                mvwchgat(win, y, start_x, end_x - start_x, A_UNDERLINE, color_pair, NULL);

                wattron(win, COLOR_PAIR(color_pair));
                if (end_x < getmaxx(win)) {
                    mvwaddch(win, y, end_x, ']');
                }
                wattroff(win, COLOR_PAIR(color_pair));
                
                // Show message in status if the cursor is on the line
                if (state->current_line == diag->range.start.line) {
                    snprintf(state->status_msg, STATUS_MSG_LEN, "[%s] %s", 
                            diag->code, diag->message);
                }
            }
        }
    }
}
