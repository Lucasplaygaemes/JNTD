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
}

void recalcular_layout_janelas() {
    int screen_rows, screen_cols;
    getmaxyx(stdscr, screen_rows, screen_cols);

    if (gerenciador.num_janelas == 0) return;

    int largura_janela = screen_cols / gerenciador.num_janelas;
    for (int i = 0; i < gerenciador.num_janelas; i++) {
        JanelaEditor *jw = gerenciador.janelas[i];
        jw->y = 0;
        jw->x = i * largura_janela;
        jw->altura = screen_rows;
        jw->largura = (i == gerenciador.num_janelas - 1) ? (screen_cols - jw->x) : largura_janela;

        if (jw->win) {
            delwin(jw->win);
        }
        jw->win = newwin(jw->altura, jw->largura, jw->y, jw->x);
        keypad(jw->win, TRUE);
        scrollok(jw->win, FALSE);
    }
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
    load_syntax_file(state, "syntaxes/c.syntax");

    gerenciador.janelas[gerenciador.num_janelas - 1] = nova_janela;
    gerenciador.janela_ativa_idx = gerenciador.num_janelas - 1;

    recalcular_layout_janelas(); // Create the ncurses window FIRST

    // Now that the window exists, we can safely load the file
    if (filename) {
        load_file(state, filename);
    } else {
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
        wnoutrefresh(jw->win);
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
    EditorState *state = jw->estado;

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


