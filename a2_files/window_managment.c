#include "window_managment.h" // Include its own header
#include "defs.h" // For EditorState, etc.
#include "fileio.h" // For save_last_line, load_file, load_syntax_file
#include "others.h" // For editor_end_completion, free_snapshot, push_undo
#include "screen_ui.h" // For editor_redraw, posicionar_cursor_ativo, get_visual_pos, editor_draw_completion_win
#include "lsp_client.h" // For lsp_shutdown
#include "direct_navigation.h"
#include <unistd.h>

// ===================================================================
//  1. Window Management
// ===================================================================

void free_editor_state(EditorState* state) {
    if (!state) return;

    if (state->filename[0] != '[') {
        save_last_line(state->filename, state->current_line);
    }
    if (state->completion_mode != COMPLETION_NONE) editor_end_completion(state);
    for(int j=0; j < state->history_count; j++) free(state->command_history[j]);
    for (int j = 0; j < state->undo_count; j++) free_snapshot(state->undo_stack[j]);
    for (int j = 0; j < state->redo_count; j++) free_snapshot(state->redo_stack[j]);
    for (int j = 0; j < state->num_syntax_rules; j++) free(state->syntax_rules[j].word);
    free(state->syntax_rules);
    for (int j = 0; j < state->num_recent_dirs; j++) {
        free(state->recent_dirs[j]->path);
        free(state->recent_dirs[j]);
    }
    
    if (state->unmatched_brackets) {
        free(state->unmatched_brackets);
        state->unmatched_brackets = NULL;
    }
    
    free(state->recent_dirs);
    for (int j = 0; j < state->num_lines; j++) {
        if (state->lines[j]) free(state->lines[j]);
    }
    free(state);
}

void free_janela_editor(JanelaEditor* jw) {
    if (!jw) return;
    free_editor_state(jw->estado);
    delwin(jw->win);
    free(jw);
}

void inicializar_gerenciador_janelas() {
    gerenciador.janelas = NULL;
    gerenciador.num_janelas = 0;
    gerenciador.janela_ativa_idx = -1;
    gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT; // Layout padrão
}

// Adicione esta nova função em a2_files/window_managment.c
void ciclar_layout() {
    if (gerenciador.num_janelas <= 1) return; // Não faz nada com 1 ou 0 janelas

    switch (gerenciador.num_janelas) {
        case 2:
            // Alterna entre Vertical e Horizontal
            if (gerenciador.current_layout == LAYOUT_VERTICAL_SPLIT) {
                gerenciador.current_layout = LAYOUT_HORIZONTAL_SPLIT;
            } else {
                gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        case 3:
            // Alterna entre Vertical e Principal + Empilhado
            if (gerenciador.current_layout == LAYOUT_VERTICAL_SPLIT) {
                gerenciador.current_layout = LAYOUT_MAIN_AND_STACK;
            } else {
                gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        case 4:
            // Alterna entre Vertical e Grade
            if (gerenciador.current_layout == LAYOUT_VERTICAL_SPLIT) {
                gerenciador.current_layout = LAYOUT_GRID;
            } else {
                gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            }
            break;
        default:
            // Para 5+ janelas, força o layout vertical
            gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
            break;
    }

    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void recalcular_layout_janelas() {
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    if (gerenciador.num_janelas == 0) return;

    // Se um layout for inválido para o número atual de janelas, volte ao padrão.
    if ((gerenciador.num_janelas != 2 && gerenciador.current_layout == LAYOUT_HORIZONTAL_SPLIT) ||
        (gerenciador.num_janelas != 3 && gerenciador.current_layout == LAYOUT_MAIN_AND_STACK) ||
        (gerenciador.num_janelas != 4 && gerenciador.current_layout == LAYOUT_GRID)) {
        gerenciador.current_layout = LAYOUT_VERTICAL_SPLIT;
    }

    switch (gerenciador.current_layout) {
        case LAYOUT_HORIZONTAL_SPLIT: // 2 Janelas: uma em cima, outra embaixo
            if (gerenciador.num_janelas == 2) {
                int altura_janela = screen_rows / 2;
                for (int i = 0; i < 2; i++) {
                    gerenciador.janelas[i]->y = i * altura_janela;
                    gerenciador.janelas[i]->x = 0;
                    gerenciador.janelas[i]->largura = screen_cols;
                    gerenciador.janelas[i]->altura = (i == 1) ? (screen_rows - altura_janela) : altura_janela;
                }
            }
            break;

        case LAYOUT_MAIN_AND_STACK: // 3 Janelas: 1 grande à esquerda, 2 pequenas à direita
            if (gerenciador.num_janelas == 3) {
                int main_width = screen_cols / 2;
                int stack_width = screen_cols - main_width;
                int stack_height = screen_rows / 2;

                // Janela principal
                gerenciador.janelas[0]->y = 0;
                gerenciador.janelas[0]->x = 0;
                gerenciador.janelas[0]->largura = main_width;
                gerenciador.janelas[0]->altura = screen_rows;

                // Duas janelas empilhadas
                gerenciador.janelas[1]->y = 0;
                gerenciador.janelas[1]->x = main_width;
                gerenciador.janelas[1]->largura = stack_width;
                gerenciador.janelas[1]->altura = stack_height;

                gerenciador.janelas[2]->y = stack_height;
                gerenciador.janelas[2]->x = main_width;
                gerenciador.janelas[2]->largura = stack_width;
                gerenciador.janelas[2]->altura = screen_rows - stack_height;
            }
            break;

        case LAYOUT_GRID: // 4 Janelas: grade 2x2
            if (gerenciador.num_janelas == 4) {
                int win_w = screen_cols / 2;
                int win_h = screen_rows / 2;
                gerenciador.janelas[0]->y = 0;     gerenciador.janelas[0]->x = 0;     // Top-left
                gerenciador.janelas[1]->y = 0;     gerenciador.janelas[1]->x = win_w; // Top-right
                gerenciador.janelas[2]->y = win_h; gerenciador.janelas[2]->x = 0;     // Bottom-left
                gerenciador.janelas[3]->y = win_h; gerenciador.janelas[3]->x = win_w; // Bottom-right

                for(int i=0; i<4; i++) {
                    gerenciador.janelas[i]->altura = (i >= 2) ? screen_rows - win_h : win_h;
                    gerenciador.janelas[i]->largura = (i % 2 != 0) ? screen_cols - win_w : win_w;
                }
            }
            break;

        case LAYOUT_VERTICAL_SPLIT: // Layout padrão (todas lado a lado)
        default: {
            int largura_janela = screen_cols / gerenciador.num_janelas;
            for (int i = 0; i < gerenciador.num_janelas; i++) {
                gerenciador.janelas[i]->y = 0;
                gerenciador.janelas[i]->x = i * largura_janela;
                gerenciador.janelas[i]->altura = screen_rows;
                gerenciador.janelas[i]->largura = (i == gerenciador.num_janelas - 1) ? (screen_cols - gerenciador.janelas[i]->x) : largura_janela;
            }
            break;
        }
    }

    // Redimensiona e recria as janelas ncurses
    for (int i = 0; i < gerenciador.num_janelas; i++) {
        JanelaEditor *jw = gerenciador.janelas[i];
        if (jw->win) {
            delwin(jw->win);
        }
        jw->win = newwin(jw->altura, jw->largura, jw->y, jw->x);
        keypad(jw->win, TRUE);
        scrollok(jw->win, FALSE);
    }
}

void rotacionar_janelas() {
    if (gerenciador.num_janelas <= 1) {
        return;
    }
    JanelaEditor *ultima_janela = gerenciador.janelas[gerenciador.num_janelas - 1];
    
    for (int i = gerenciador.num_janelas - 1; i > 0; i--) {
        gerenciador.janelas[i] = gerenciador.janelas[i - 1];
    }
    
    gerenciador.janelas[0] = ultima_janela;
    
    gerenciador.janela_ativa_idx = (gerenciador.janela_ativa_idx + 1) % gerenciador.num_janelas;
    
    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void mover_janela_para_posicao(int target_idx) {
    int active_idx = gerenciador.janela_ativa_idx;

    // Validação: verifica se o movimento é possível e necessário
    if (gerenciador.num_janelas <= 1 || target_idx < 0 || target_idx >= gerenciador.num_janelas || target_idx == active_idx) {
        return;
    }

    // Troca os ponteiros no array
    JanelaEditor *janela_ativa_ptr = gerenciador.janelas[active_idx];
    gerenciador.janelas[active_idx] = gerenciador.janelas[target_idx];
    gerenciador.janelas[target_idx] = janela_ativa_ptr;

    // Atualiza o índice da janela ativa, pois ela se moveu
    gerenciador.janela_ativa_idx = target_idx;

    recalcular_layout_janelas();
    redesenhar_todas_as_janelas();
}

void criar_nova_janela(const char *filename) {
    gerenciador.num_janelas++;
    gerenciador.janelas = realloc(gerenciador.janelas, sizeof(JanelaEditor*) * gerenciador.num_janelas);

    JanelaEditor *nova_janela = calloc(1, sizeof(JanelaEditor));
    nova_janela->estado = calloc(1, sizeof(EditorState));
    
    EditorState *state = nova_janela->estado;
    strcpy(state->filename, "[No Name]");
    state->mode = NORMAL; 
    state->completion_mode = COMPLETION_NONE;
    state->unmatched_brackets = NULL;
    state->num_unmatched_brackets = 0;
    state->last_search[0] = '\0';
    state->last_match_line = -1;
    state->last_match_col = -1;
    state->buffer_modified = false;
    state->last_file_mod_time = 0;
    state->undo_count = 0;
    state->redo_count = 0;
    state->last_auto_save_time = time(NULL);
    state->auto_indent_on_newline = true;
    state->paste_mode = false;
    state->word_wrap_enabled = true;
    state->num_recent_dirs = 0;
    state->recent_dirs = NULL;
    load_directory_history(state);

    char initial_cwd[1024];
    if (getcwd(initial_cwd, sizeof(initial_cwd)) != NULL) {
        update_directory_access(state, initial_cwd);
    }
    
    gerenciador.janelas[gerenciador.num_janelas - 1] = nova_janela;
    gerenciador.janela_ativa_idx = gerenciador.num_janelas - 1;

    recalcular_layout_janelas(); // Create the ncurses window FIRST

    // Now that the window exists, we can safely load the file
    if (filename) {
        load_file(state, filename);
    } else {
        // For a new, empty file, load a default syntax
        load_syntax_file(state, "c.syntax");
        state->lines[0] = calloc(1, 1);
        state->num_lines = 1;
    }

    push_undo(state);
}

void redesenhar_todas_as_janelas() {
    erase();
    wnoutrefresh(stdscr);

    for (int i = 0; i < gerenciador.num_janelas; i++) {
        JanelaEditor *jw = gerenciador.janelas[i];
        editor_redraw(jw->win, jw->estado);
    }

    // Lógica para desenhar o pop-up de diagnóstico por último
    if (gerenciador.num_janelas > 0) {
        JanelaEditor* active_jw = gerenciador.janelas[gerenciador.janela_ativa_idx];
        EditorState* state = active_jw->estado;
        if (state->lsp_enabled) {
            LspDiagnostic *diag = get_diagnostic_under_cursor(state);
            if (diag) {
                // A mensagem de status já foi definida em editor_redraw
                draw_diagnostic_popup(active_jw->win, state, diag->message);
            }
        }
        // Limpa a mensagem de status da janela ativa após o desenho
        state->status_msg[0] = '\0';
    }

    posicionar_cursor_ativo();
    doupdate();
}

void posicionar_cursor_ativo() {
    if (gerenciador.num_janelas == 0) return;

    JanelaEditor* active_jw = gerenciador.janelas[gerenciador.janela_ativa_idx];
    EditorState* state = active_jw->estado;
    WINDOW* win = active_jw->win;

    if (state->completion_mode != COMPLETION_NONE) {
        editor_draw_completion_win(win, state);
    } else {
        curs_set(1);
        if (state->mode == COMMAND) {
            int rows, cols;
            getmaxyx(win, rows, cols);
            wmove(win, rows - 1, state->command_pos + 2);
        } else {
            int visual_y, visual_x;
            get_visual_pos(win, state, &visual_y, &visual_x);
            
            // Account for window borders correctly
            int border_offset = gerenciador.num_janelas > 1 ? 1 : 0;
            int screen_y = visual_y - state->top_line + border_offset;
            int screen_x = visual_x - state->left_col + border_offset;
            
            // Ensure cursor stays within window bounds
            int max_y, max_x;
            getmaxyx(win, max_y, max_x);
            if (screen_y >= max_y) screen_y = max_y - 1;
            if (screen_x >= max_x) screen_x = max_x - 1;
            if (screen_y < border_offset) screen_y = border_offset;
            if (screen_x < border_offset) screen_x = border_offset;
            
            wmove(win, screen_y, screen_x);
        }
    }
}
void fechar_janela_ativa(bool *should_exit) {
    if (gerenciador.num_janelas == 0) return;

    int idx = gerenciador.janela_ativa_idx;
    JanelaEditor *jw = gerenciador.janelas[idx];
    EditorState *state = gerenciador.janelas[gerenciador.janela_ativa_idx]->estado;
    if (state->lsp_enabled) {
        lsp_shutdown(state);
    }
    if (state->buffer_modified) {
        snprintf(state->status_msg, sizeof(state->status_msg), "Warning: Unsaved changes! Use :q! to force quit.");
        return;
    }

    free_janela_editor(jw);

    for (int i = idx; i < gerenciador.num_janelas - 1; i++) {
        gerenciador.janelas[i] = gerenciador.janelas[i+1];
    }
    gerenciador.num_janelas--;

    if (gerenciador.num_janelas == 0) {
        *should_exit = true;
        free(gerenciador.janelas);
        gerenciador.janelas = NULL;
        return;
    }

    JanelaEditor **new_janelas = realloc(gerenciador.janelas, sizeof(JanelaEditor*) * gerenciador.num_janelas);
    if (!new_janelas) {
        endwin();
        fprintf(stderr, "Error: Failed to reallocate window manager memory.\n");
        exit(1);
    }
    gerenciador.janelas = new_janelas;

    if (gerenciador.janela_ativa_idx >= gerenciador.num_janelas) {
        gerenciador.janela_ativa_idx = gerenciador.num_janelas - 1;
    }

    recalcular_layout_janelas();
}

void proxima_janela() {
    if (gerenciador.num_janelas > 1) {
        gerenciador.janela_ativa_idx = (gerenciador.janela_ativa_idx + 1) % gerenciador.num_janelas;
    }
}

void janela_anterior() {
    if (gerenciador.num_janelas > 1) {
        gerenciador.janela_ativa_idx = (gerenciador.janela_ativa_idx - 1 + gerenciador.num_janelas) % gerenciador.num_janelas;
    }
}


