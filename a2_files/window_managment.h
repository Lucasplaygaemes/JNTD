#ifndef WINDOW_MANAGMENT_H
#define WINDOW_MANAGMENT_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for window_managment.c
void free_editor_state(EditorState* state);
void free_janela_editor(JanelaEditor* jw);
void inicializar_gerenciador_janelas();
void recalcular_layout_janelas();
void criar_nova_janela(const char *filename);
void redesenhar_todas_as_janelas();
void posicionar_cursor_ativo();
void fechar_janela_ativa(bool *should_exit);
void proxima_janela();
void janela_anterior();

#endif // WINDOW_MANAGMENT_H