#ifndef WINDOW_MANAGMENT_H
#define WINDOW_MANAGMENT_H

#include "defs.h" // For EditorState, etc.

// Function prototypes for window_managment.c
void free_editor_state(EditorState* state);
void free_janela_editor(JanelaEditor* jw);
void free_workspace(GerenciadorJanelas *ws);
void inicializar_workspaces();
void recalcular_layout_janelas();
void criar_nova_janela(const char *filename);
void redesenhar_todas_as_janelas();
void posicionar_cursor_ativo();
void fechar_janela_ativa(bool *should_exit);
void proxima_janela();
void janela_anterior();
void ciclar_layout();
void rotacionar_janelas();
void mover_janela_para_posicao(int target_idx);
void display_recent_files();
void criar_novo_workspace();
void ciclar_workspaces(int direcao);
void mover_janela_para_workspace(int target_idx);
void fechar_workspace_ativo(bool *should_exit);
void prompt_and_create_gdb_workspace();
void executar_comando_em_novo_workspace(const char *comando_str);

void gf2_starter();



#endif // WINDOW_MANAGMENT_H
