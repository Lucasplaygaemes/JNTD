#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "plugin.h"

#define MAX_INPUT 256
#define MAX_TERMS 10
#define PI 3.14159265359
#define EPSILON 0.0001
#define TRAPEZOID_STEPS 1000

// Estrutura para polinômios
typedef struct {
    double coef;
    int exp;
} Term;

// Estrutura para funções trigonométricas
typedef struct {
    double coef;
    char type[10];
} TrigTerm;

// Funções matemáticas
double eval_polynomial(Term *terms, int num_terms, double x) {
    double result = 0.0;
    for (int i = 0; i < num_terms; i++) {
        result += terms[i].coef * pow(x, terms[i].exp);
    }
    return result;
}

double eval_trig(TrigTerm *trig_terms, int num_trig, double x) {
    double result = 0.0;
    for (int i = 0; i < num_trig; i++) {
        if (strcmp(trig_terms[i].type, "sin") == 0) {
            result += trig_terms[i].coef * sin(x);
        } else if (strcmp(trig_terms[i].type, "cos") == 0) {
            result += trig_terms[i].coef * cos(x);
        } else if (strcmp(trig_terms[i].type, "tan") == 0) {
            result += trig_terms[i].coef * tan(x);
        }
    }
    return result;
}

void derive_polynomial(Term *terms, int num_terms, Term *result) {
    int result_idx = 0;
    for (int i = 0; i < num_terms; i++) {
        if (terms[i].exp > 0) {
            result[result_idx].coef = terms[i].coef * terms[i].exp;
            result[result_idx].exp = terms[i].exp - 1;
            result_idx++;
        }
    }
    for (int i = result_idx; i < num_terms; i++) {
        result[i].coef = 0;
        result[i].exp = 0;
    }
}

void derive_trig(TrigTerm *trig_terms, int num_trig, TrigTerm *result) {
    for (int i = 0; i < num_trig; i++) {
        result[i].coef = trig_terms[i].coef;
        if (strcmp(trig_terms[i].type, "sin") == 0) {
            strcpy(result[i].type, "cos");
        } else if (strcmp(trig_terms[i].type, "cos") == 0) {
            result[i].coef = -trig_terms[i].coef;
            strcpy(result[i].type, "sin");
        } else if (strcmp(trig_terms[i].type, "tan") == 0) {
            result[i].coef = trig_terms[i].coef;
            strcpy(result[i].type, "sec2");
        }
    }
}

// Funções de exibição
void print_polynomial(Term *terms, int num_terms) {
    int first = 1;
    for (int i = 0; i < num_terms; i++) {
        if (terms[i].coef != 0) {
            if (!first && terms[i].coef > 0) printf(" + ");
            else if (terms[i].coef < 0) printf(" - ");
            if (first && terms[i].coef < 0) printf("-");
            
            if (fabs(terms[i].coef) != 1 || terms[i].exp == 0) 
                printf("%.2f", fabs(terms[i].coef));
            
            if (terms[i].exp > 0) {
                printf("x");
                if (terms[i].exp > 1) printf("^%d", terms[i].exp);
            }
            first = 0;
        }
    }
    if (first) printf("0");
}

void print_trig(TrigTerm *trig_terms, int num_trig) {
    int first = 1;
    for (int i = 0; i < num_trig; i++) {
        if (trig_terms[i].coef != 0) {
            if (!first && trig_terms[i].coef > 0) printf(" + ");
            else if (trig_terms[i].coef < 0) printf(" - ");
            if (first && trig_terms[i].coef < 0) printf("-");
            
            if (fabs(trig_terms[i].coef) != 1) 
                printf("%.2f", fabs(trig_terms[i].coef));
            
            printf("%s(x)", trig_terms[i].type);
            first = 0;
        }
    }
    if (first) printf("0");
}

// Cálculos avançados
double calculate_limit(Term *terms, int num_terms, TrigTerm *trig_terms, int num_trig, double a) {
    double left = a - EPSILON;
    double right = a + EPSILON;
    double val_left = eval_polynomial(terms, num_terms, left) + eval_trig(trig_terms, num_trig, left);
    double val_right = eval_polynomial(terms, num_terms, right) + eval_trig(trig_terms, num_trig, right);
    return (val_left + val_right) / 2.0;
}

double integrate_trapezoid(Term *terms, int num_terms, TrigTerm *trig_terms, int num_trig, double a, double b) {
    double h = (b - a) / TRAPEZOID_STEPS;
    double sum = 0.0;
    for (int i = 0; i < TRAPEZOID_STEPS; i++) {
        double x0 = a + i * h;
        double x1 = a + (i + 1) * h;
        double f0 = eval_polynomial(terms, num_terms, x0) + eval_trig(trig_terms, num_trig, x0);
        double f1 = eval_polynomial(terms, num_terms, x1) + eval_trig(trig_terms, num_trig, x1);
        sum += (f0 + f1) * h / 2.0;
    }
    return sum;
}

// Função principal do plugin
void calc_execute(const char* args) {
    printf("Calculadora executando: %s\n", args);
    
    // Simulação de parsing de argumentos
    char* argv[20];
    int argc = 0;
    char input_copy[MAX_INPUT];
    
    strncpy(input_copy, args, MAX_INPUT);
    char* token = strtok(input_copy, " ");
    
    while (token != NULL && argc < 20) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    
    if (argc == 0) {
        printf("Uso: calc <operacao> <parametros>\n");
        printf("Operações disponíveis:\n");
        printf("  soma <num1> <num2> - Soma dois números\n");
        printf("  sub <num1> <num2> - Subtrai dois números\n");
        // ... (outras operações)
        return;
    }

    // Operações básicas
    if (strcmp(argv[0], "soma") == 0 && argc == 3) {
        double num1 = atof(argv[1]);
        double num2 = atof(argv[2]);
        printf("Resultado: %.2f + %.2f = %.2f\n", num1, num2, num1 + num2);
    } 
    else if (strcmp(argv[0], "sub") == 0 && argc == 3) {
        double num1 = atof(argv[1]);
        double num2 = atof(argv[2]);
        printf("Resultado: %.2f - %.2f = %.2f\n", num1, num2, num1 - num2);
    } 
    // ... (outras operações)
    else {
        printf("Comando inválido ou argumentos incorretos\n");
    }
}

// Registro do plugin
Plugin plugin = {
    .name = "calc",
    .execute = calc_execute
};
