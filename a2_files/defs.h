#ifndef DEFS_H
#define DEFS_H

#define _XOPEN_SOURCE 700
#define NCURSES_WIDECHAR 1
#include <limits.h> // For PATH_MAX

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <jansson.h>
#include <vterm.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#define KEY_CTRL_P 16
#define KEY_CTRL_DEL 520
#define KEY_CTRL_K 11
#define KEY_CTRL_F 6
#define KEY_CTRL_D 4
#define KEY_CTRL_A 1
#define KEY_CTRL_G 7
#define KEY_CTRL_W 23
#define KEY_CTRL_RIGHT_BRACKET 29
#define KEY_CTRL_LEFT_BRACKET 27

#define LSP_SEVERITY_ERROR 1
#define LSP_SEVERITY_WARNING 2
#define LSP_SEVERITY_INFO 3
#define LSP_SEVERITY_HINT 4

#define MAX_LINES 16486
#define MAX_LINE_LEN 4096
#define STATUS_MSG_LEN 250
#define PAGE_JUMP 10
#define TAB_SIZE 4
#define MAX_COMMAND_HISTORY 50
#define AUTO_SAVE_INTERVAL 1
#define AUTO_SAVE_EXTENSION ".sv"
#define MAX_UNDO_LEVELS 512

struct EditorState;

#ifndef LSPMESSAGE_DEFINED
#define LSPMESSAGE_DEFINED
typedef struct {
    char *jsonrpc;
    char *id;
    char *method;
    json_t *params;
    json_t *result;
    json_t *error;
} LspMessage;
#endif

typedef enum {
    TIPOJANELA_EDITOR,
    TIPOJANELA_TERMINAL
} TipoJanela;


#ifndef EDITORMODE_DEFINED
#define EDITORMODE_DEFINED
typedef enum {
    NORMAL,
    INSERT,
    COMMAND,
    VISUAL,
    OPERATOR_PENDING
} EditorMode;
#endif

#ifndef COMPLETIONMODE_DEFINED
#define COMPLETIONMODE_DEFINED
typedef enum {
    COMPLETION_NONE,
    COMPLETION_TEXT,
    COMPLETION_COMMAND,
    COMPLETION_FILE
} CompletionMode;
#endif

#ifndef FILERECOVERYCHOICE_DEFINED
#define FILERECOVERYCHOICE_DEFINED
typedef enum {
    RECOVER_FROM_SV,
    RECOVER_OPEN_ORIGINAL,
    RECOVER_DIFF,
    RECOVER_IGNORE,
    RECOVER_ABORT
} FileRecoveryChoice;
#endif

#ifndef VISUALSELECTIONMODE_DEFINED
#define VISUALSELECTIONMODE_DEFINED
typedef enum {
    VISUAL_MODE_NONE,
    VISUAL_MODE_YANK,
    VISUAL_MODE_SELECT
} VisualSelectionMode;
#endif

#ifndef EDITORSNAPSHOT_DEFINED
#define EDITORSNAPSHOT_DEFINED
typedef struct {
    char **lines;
    int num_lines;
    int current_line;
    int current_col;
    int ideal_col;
    int top_line;
    int left_col;
} EditorSnapshot;
#endif

#ifndef SYNTAXRULETYPE_DEFINED
#define SYNTAXRULETYPE_DEFINED
enum SyntaxRuleType { 
    SYNTAX_KEYWORD,
    SYNTAX_TYPE,
    SYNTAX_STD_FUNCTION
};
#endif

#ifndef SYNTAXRULE_DEFINED
#define SYNTAXRULE_DEFINED
typedef struct {
    char *word;
    enum SyntaxRuleType type;
} SyntaxRule;
#endif

#ifndef BRACKETINFO_DEFINED
#define BRACKETINFO_DEFINED
typedef struct {
    int line;
    int col;
    char type;
} BracketInfo;
#endif

#ifndef DIRECTORYINFO_DEFINED
#define DIRECTORYINFO_DEFINED
typedef struct {
    char *path;
    int access_count;
} DirectoryInfo;
#endif

#ifndef LSPPOSITION_DEFINED
#define LSPPOSITION_DEFINED
typedef struct {
    int line;
    int character;
} LspPosition;
#endif

#ifndef LSPRANGE_DEFINED
#define LSPRANGE_DEFINED
typedef struct {
    LspPosition start;
    LspPosition end;
} LspRange;
#endif

#ifndef LSPDIAGNOSTIC_DEFINED
#define LSPDIAGNOSTIC_DEFINED
typedef struct {
    LspRange range;
    int severity;
    char *message;
    char *code;
} LspDiagnostic;
#endif

#ifndef LSPCLIENT_DEFINED
#define LSPCLIENT_DEFINED
typedef struct {
    pid_t server_pid;
    int stdin_fd;
    int stdout_fd;
    int stderr_fd;
    bool initialized;
    char *rootUri;
    char *workspaceFolders;
    char *languageId;
    char *compilerFlags;
    char *compilationDatabase;
} LspClient;
#endif

#ifndef LSPDOCUMENTSTATE_DEFINED
#define LSPDOCUMENTSTATE_DEFINED
typedef struct {
    char *uri;
    int version;
    LspDiagnostic *diagnostics;
    int diagnostics_count;
    bool needs_update;
} LspDocumentState;
#endif

#ifndef FILEINFO_DEFINED
#define FILEINFO_DEFINED
typedef struct {
    char *path;
    int access_count;
} FileInfo;
#endif

#ifndef EDITORSTATE_DEFINED
#define EDITORSTATE_DEFINED
typedef struct EditorState {
    int selection_start_line;
    int selection_start_col;
    VisualSelectionMode visual_selection_mode;
    char* yank_register;
    char* move_register;
    bool is_moving;
    char *lines[MAX_LINES];
    int num_lines, current_line, current_col, ideal_col, top_line, left_col, command_pos;
    EditorMode mode;
    char filename[256], status_msg[STATUS_MSG_LEN], command_buffer[100];
    char *command_history[MAX_COMMAND_HISTORY];
    int history_count, history_pos;
    CompletionMode completion_mode;
    char **completion_suggestions;
    int num_suggestions, selected_suggestion, completion_start_col, completion_scroll_top;
    WINDOW *completion_win;
    char word_to_complete[100];
    SyntaxRule *syntax_rules;
    int num_syntax_rules;
    char last_search[100];
    int last_match_line, last_match_col;
    bool buffer_modified;
    time_t last_file_mod_time;
    EditorSnapshot *undo_stack[MAX_UNDO_LEVELS];
    int undo_count;
    EditorSnapshot *redo_stack[MAX_UNDO_LEVELS];
    int redo_count;
    time_t last_auto_save_time;
    bool auto_indent_on_newline;
    bool paste_mode;
    bool word_wrap_enabled;
    DirectoryInfo **recent_dirs;
    int num_recent_dirs;
    BracketInfo *unmatched_brackets;
    int num_unmatched_brackets;
    LspClient *lsp_client;
    LspDocumentState *lsp_document;
    bool lsp_enabled;
    WINDOW *diagnostic_popup;
    time_t last_change_time;
    time_t lsp_init_time;
    int lsp_init_retries;
    FileInfo **recent_files;
    int num_recent_files;

    // Macro recording
    char* macro_registers[26];
    bool is_recording_macro;
    int recording_register_idx; // 0-25 for a-z
    
    char pending_operator;
    
    char last_played_macro_register;
    bool single_command_mode;    

} EditorState;
#endif

#ifndef JANELA_EDITOR_DEFINED
#define JANELA_EDITOR_DEFINED
typedef struct JanelaEditor {
    WINDOW *win;
    int y, x, altura, largura;
    TipoJanela tipo;

    EditorState *estado; // Used only by TIPOJANELA_EDITOR

    int pty_fd;          // Used only by TIPOJANELA_TERMINAL
    pid_t pid;           // Used only by TIPOJANELA_TERMINAL
    vterm_t *vterm;      // The only field from libvterm that we need!
} JanelaEditor;
#endif

#ifndef LAYOUT_MODE_DEFINED
#define LAYOUT_MODE_DEFINED

typedef enum {
    LAYOUT_VERTICAL_SPLIT,
    LAYOUT_HORIZONTAL_SPLIT,
    LAYOUT_MAIN_AND_STACK,
    LAYOUT_GRID
} LayoutMode;
#endif

#ifndef GERENCIADOR_JANELAS_DEFINED
#define GERENCIADOR_JANELAS_DEFINED
typedef struct {
    JanelaEditor **janelas;
    int num_janelas;
    int janela_ativa_idx;
    LayoutMode current_layout;
} GerenciadorJanelas;
#endif

#ifndef GERENCIADOR_WORKSPACES_DEFINED
#define GERENCIADOR_WORKSPACES_DEFINED
typedef struct {
    GerenciadorJanelas **workspaces;
    int num_workspaces;
    int workspace_ativo_idx;
} GerenciadorWorkspaces;
#endif

#define ACTIVE_WS (gerenciador_workspaces.workspaces[gerenciador_workspaces.workspace_ativo_idx])

#ifndef COMMANDINFO_DEFINED
#define COMMANDINFO_DEFINED
typedef struct {
    const char *command;
    const char *description;
} CommandInfo;
#endif

#ifndef FILEVIEWER_DEFINED
#define FILEVIEWER_DEFINED
typedef struct {
    char **lines;
    int num_lines;
} FileViewer;
#endif

extern GerenciadorWorkspaces gerenciador_workspaces;

extern char executable_dir[PATH_MAX];
extern char* global_yank_register;

#endif // DEFS_H
