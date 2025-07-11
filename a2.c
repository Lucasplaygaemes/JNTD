#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int ch;
char buffer[4098];
int buffer_index = 0;

//inicializa o ncurse com o necessario//
void inicializar_ncurses() {
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(1); // mostra o cursor no modo editor
	start_color();
	clear();

	//Define as cores//	
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(2, COLOR_BLACK, COLOR_BLACK);
}

void save_file(const char *filename, const char *content) {
	FILE *file = fopen(filename, "w");
	if (file != NULL) {
		fputs(content, file);
		fclose(file);
	}
}

int editor_mode() {
	curs_set(1);
	clear();
	printw("Modo editor... (Pressione ESC para salvar e sair)\n");
	refresh();

	memset(buffer, 0, sizeof(buffer));
	buffer_index = 0;

	while ((ch = getch()) != 27) { // é o codigo ASCII para ESC
		switch(ch) {
			case KEY_BACKSPACE:
			case 127:
				if (buffer_index > 0) {
					buffer_index--;
					int y, x;
					getyx(stdscr, y, x);
					mvdelch(y, x - 1);
					refresh();
				}
				break;
			default:
				if (isprint(ch) && buffer_index < sizeof(buffer) - 1) {
					buffer[buffer_index++] = ch;
					addch(ch);
					refresh();
				}
				break;
			case '\n':
			case KEY_ENTER:
				if (buffer_index < sizeof(buffer) - 1) {
					buffer[buffer_index++] = '\n';
					addch('\n');
					refresh();
					break;
				}
		}
	}
	buffer[buffer_index] = '\0';

	char filename[256];
	int rows, cols;
	getmaxyx(stdscr, rows, cols);
	
	echo(); // ativa echo para ver o nome do arquivo
	mvprintw(rows - 1, 0, "Nome do arquivo para ser salvo: ");
	getstr(filename);
	noecho();

	if (strlen(filename) > 0) {
		save_file(filename, buffer);
		mvprintw(rows - 1, 0, "Arquivo salvo como '%s'. Aperte qualquer tecla para sair.", filename);
	} else {
		mvprintw(rows - 1, 0, "Salvamento cancelado, aperte qualquer tecla para sair.");
	}
	
	curs_set(0); // Esconde o cursos antes de sair
	getch();
	return 0;
}

int main() {
	inicializar_ncurses();
	curs_set(0); // Esconde o cursor no meunu
	printw("Bem vindo ao A2 o editor de texto do JTND.\n");
	printw("Pressione 'e' para entrar no modo editor.\n");
	refresh();
	
	char texto = getch();

	if (texto == 'e') {
		editor_mode();
	}
	
	endwin();
	return 0;
}

