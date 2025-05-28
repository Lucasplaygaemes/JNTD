#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


#define MAX_INPUT 256
#define MAX_TERMS 10
#define PI 3.14159265359
#define EPSILON 0.0001

// Estru para representar polinomio ex 3x^2////

typedef struct {
	double coef;
	int exp;
	//Coeficiente e Expoente respectivamente//

} Term;

// Estrutura para representar func trigonometricas ex 2*sin(x)//

typedef struct {
	double coef;
	char type[10];
} TrigTerm;

// Função para avaliar um polinomio em um ponto x//

double eval_p(Term *terms, int num_t, double x) {
	double result = 0.0;
	for(int = 0; i < num_t; i++) {
		result += terms[i].coef * pow(x, terms[i].exp);

	}
	return result;
}

// Funcao para avaliar uma func trigo em x//

double eval_trig(TrigTerm * trig_terms, int num_trig, double x) {
	double result = 0.0;
	for(int i = 0; i < num_t; i++) {
		if(strcmp(trig_terms[i].type, "sin") == 0) {
			result += trig_terms[i].coef * sin(x);

		} else if(strcmp(trig_terms[i].type, "cos") == 0) {
			result += trig_terms[i].coef * cos(x);

		} else if(strcmp(trig_terms[i].type, " tan") == 0) {
			result += trig_terms[i].coef * tan(x);
		}
	}
	return result;
}

// função para derivar um polinomio //


void derive_poly(Term *terms, int num_terms, Term *result) {
	int result_idx = 0;
	for(int i = 0; i < num_terms; i++) {
		if(terms[i].exp > 0) {
			result[result_idx].coef = terms[i].coef * terms[i].exp;
			result[result_idx].exp = terms[i].exp - 1;
			result_idx++;
		}
	}
	for(int i = result_idx; i < num_terms; i++) {
		result[i].coef = 0;
		result[i].exp = 0;
	}
}

// função para derivar fun trigo//

void d_t(TrigTerm *trig_terms, int num_trig, TrigTerm *result) {
	for(int i = 0; i < num_trig; i++) {
		result[i].coef = trig_terms[i].coef;
		if(strcmp(trig_terms[i].type, "sin" ) == 0) {
			strcpy(result_terms[i].type, "cos");
		} else if(strcmp(trig_terms[i].type, "cos") == 0) {
			result[i].coef = -trig_terms[i].coef;
			strcpy(result[i].type, "sin");
		} else if(strcmp(trig_terms[i].type, "tan") == 0) {
			result[i].coef = trig_terms[i].coef;
			strcpy(result[i].type, "sec2");
		}

	}
}

// funcao para exibir um polinomio
