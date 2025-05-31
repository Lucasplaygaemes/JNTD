#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_INPUT 256
#define MAX_TERMS 10
#define PI 3.14159265359
#define EPSILON 0.0001 // Para cálculos numéricos de limites
#define TRAPEZOID_STEPS 1000 // Número de passos para integração numérica

// Estrutura para representar um termo de um polinômio (ex.: 3x^2)
typedef struct {
    double coef; // Coeficiente (ex.: 3)
    int exp;     // Expoente (ex.: 2)
} Term;

// Estrutura para representar funções trigonométricas (ex.: 2*sin(x))
typedef struct {
    double coef;   // Coeficiente (ex.: 2)
    char type[10]; // Tipo: "sin", "cos", "tan"
} TrigTerm;

// Função para avaliar um polinômio em um ponto x
double eval_polynomial(Term *terms, int num_terms, double x) {
    double result = 0.0;
    for (int i = 0; i < num_terms; i++) {
        result += terms[i].coef * pow(x, terms[i].exp);
    }
    return result;
}

// Função para avaliar uma função trigonométrica em um ponto x
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

// Função para derivar um polinômio
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

// Função para derivar funções trigonométricas
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
            strcpy(result[i].type, "sec2"); // tan'(x) = sec^2(x), representado simbolicamente
        }
    }
}

// Função para exibir um polinômio
void print_polynomial(Term *terms, int num_terms) {
    int first = 1;
    for (int i = 0; i < num_terms; i++) {
        if (terms[i].coef != 0) {
            if (!first && terms[i].coef > 0) printf(" + ");
            else if (terms[i].coef < 0) printf(" - ");
            if (first && terms[i].coef < 0) printf("-");
            
            if (fabs(terms[i].coef) != 1 || terms[i].exp == 0) printf("%.2f", fabs(terms[i].coef));
            else if (terms[i].coef == -1 && terms[i].exp != 0) {
                // Não usar printf(""), apenas não imprimimos o coeficiente
            } else {
                printf("1");
            }
            
            if (terms[i].exp > 0) {
                printf("x");
                if (terms[i].exp > 1) printf("^%d", terms[i].exp);
            }
            first = 0;
        }
    }
    if (first) printf("0");
}

// Função para exibir funções trigonométricas
void print_trig(TrigTerm *trig_terms, int num_trig) {
    int first = 1;
    for (int i = 0; i < num_trig; i++) {
        if (trig_terms[i].coef != 0) {
            if (!first && trig_terms[i].coef > 0) printf(" + ");
            else if (trig_terms[i].coef < 0) printf(" - ");
            if (first && trig_terms[i].coef < 0) printf("-");
            
            if (fabs(trig_terms[i].coef) != 1) printf("%.2f", fabs(trig_terms[i].coef));
            else if (trig_terms[i].coef == -1) {
                // Não usar printf(""), apenas não imprimimos o coeficiente
            } else {
                printf("1");
            }
            
            printf("%s(x)", trig_terms[i].type);
            first = 0;
        }
    }
    if (first) printf("0");
}

// Função para calcular limite numericamente (aproximação)
double calculate_limit(Term *terms, int num_terms, TrigTerm *trig_terms, int num_trig, double a) {
    double left = a - EPSILON;
    double right = a + EPSILON;
    double val_left = eval_polynomial(terms, num_terms, left) + eval_trig(trig_terms, num_trig, left);
    double val_right = eval_polynomial(terms, num_terms, right) + eval_trig(trig_terms, num_trig, right);
    // Média dos valores para aproximação simples
    return (val_left + val_right) / 2.0;
}

// Função para integração numérica usando a regra do trapézio
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

// Função principal da calculadora
int main(int argc, char *argv[]) {
    if (argc == 1) {
        printf("Uso: ./calc <operacao> <parametros>\n");
        printf("Operacoes disponiveis:\n");
        printf("  soma <num1> <num2> - Soma dois números\n");
        printf("  sub <num1> <num2> - Subtrai dois números\n");
        printf("  mult <num1> <num2> - Multiplica dois números\n");
        printf("  div <num1> <num2> - Divide dois números\n");
        printf("  deriv_poly <coef1> <exp1> <coef2> <exp2> ... - Derivada de polinômio (até 5 termos)\n");
        printf("  deriv_trig <coef> <tipo> - Derivada de função trigonométrica (tipo: sin, cos, tan)\n");
        printf("  limit <a> <coef1> <exp1> ... / <coef> <tipo> - Limite em x=a (polinômio ou trigonométrica)\n");
        printf("  integ <a> <b> <coef1> <exp1> ... / <coef> <tipo> - Integral de a a b (polinômio ou trigonométrica)\n");
        return 1;
    }

    if (strcmp(argv[1], "soma") == 0 && argc == 4) {
        double num1 = atof(argv[2]);
        double num2 = atof(argv[3]);
        printf("Resultado: %.2f + %.2f = %.2f\n", num1, num2, num1 + num2);
    } else if (strcmp(argv[1], "sub") == 0 && argc == 4) {
        double num1 = atof(argv[2]);
        double num2 = atof(argv[3]);
        printf("Resultado: %.2f - %.2f = %.2f\n", num1, num2, num1 - num2);
    } else if (strcmp(argv[1], "mult") == 0 && argc == 4) {
        double num1 = atof(argv[2]);
        double num2 = atof(argv[3]);
        printf("Resultado: %.2f * %.2f = %.2f\n", num1, num2, num1 * num2);
    } else if (strcmp(argv[1], "div") == 0 && argc == 4) {
        double num1 = atof(argv[2]);
        double num2 = atof(argv[3]);
        if (num2 != 0) {
            printf("Resultado: %.2f / %.2f = %.2f\n", num1, num2, num1 / num2);
        } else {
            printf("Erro: Divisão por zero!\n");
        }
    } else if (strcmp(argv[1], "deriv_poly") == 0 && argc >= 4 && argc <= 12 && (argc - 2) % 2 == 0) {
        int num_terms = (argc - 2) / 2;
        Term terms[MAX_TERMS] = {0};
        Term result[MAX_TERMS] = {0};
        
        for (int i = 0; i < num_terms; i++) {
            terms[i].coef = atof(argv[2 + 2 * i]);
            terms[i].exp = atoi(argv[3 + 2 * i]);
        }
        
        printf("Polinômio original: ");
        print_polynomial(terms, MAX_TERMS);
        printf("\n");
        derive_polynomial(terms, MAX_TERMS, result);
        printf("Derivada: ");
        print_polynomial(result, MAX_TERMS);
        printf("\n");
    } else if (strcmp(argv[1], "deriv_trig") == 0 && argc == 4) {
        TrigTerm trig_terms[1] = {0};
        TrigTerm result[1] = {0};
        trig_terms[0].coef = atof(argv[2]);
        strncpy(trig_terms[0].type, argv[3], sizeof(trig_terms[0].type) - 1);
        printf("Função original: ");
        print_trig(trig_terms, 1);
        printf("\n");
        derive_trig(trig_terms, 1, result);
        printf("Derivada: ");
        print_trig(result, 1);
        printf("\n");
    } else if (strcmp(argv[1], "limit") == 0 && argc >= 4) {
        double a = atof(argv[2]);
        Term terms[MAX_TERMS] = {0};
        TrigTerm trig_terms[MAX_TERMS] = {0};
        int num_terms = 0, num_trig = 0;
        int is_trig = 0;
        for (int i = 3; i < argc; i += 2) {
            if (strcmp(argv[i], "/") == 0) {
                is_trig = 1;
                i++;
            }
            if (!is_trig && i + 1 < argc) {
                terms[num_terms].coef = atof(argv[i]);
                terms[num_terms].exp = atoi(argv[i + 1]);
                num_terms++;
            } else if (is_trig && i + 1 < argc) {
                trig_terms[num_trig].coef = atof(argv[i]);
                strncpy(trig_terms[num_trig].type, argv[i + 1], sizeof(trig_terms[num_trig].type) - 1);
                num_trig++;
            }
        }
        double limit_val = calculate_limit(terms, num_terms, trig_terms, num_trig, a);
        printf("Limite aproximado em x = %.2f: %.2f\n", a, limit_val);
    } else if (strcmp(argv[1], "integ") == 0 && argc >= 5) {
        double a = atof(argv[2]);
        double b = atof(argv[3]);
        Term terms[MAX_TERMS] = {0};
        TrigTerm trig_terms[MAX_TERMS] = {0};
        int num_terms = 0, num_trig = 0;
        int is_trig = 0;
        for (int i = 4; i < argc; i += 2) {
            if (strcmp(argv[i], "/") == 0) {
                is_trig = 1;
                i++;
            }
            if (!is_trig && i + 1 < argc) {
                terms[num_terms].coef = atof(argv[i]);
                terms[num_terms].exp = atoi(argv[i + 1]);
                num_terms++;
            } else if (is_trig && i + 1 < argc) {
                trig_terms[num_trig].coef = atof(argv[i]);
                strncpy(trig_terms[num_trig].type, argv[i + 1], sizeof(trig_terms[num_trig].type) - 1);
                num_trig++;
            }
        }
        double integral_val = integrate_trapezoid(terms, num_terms, trig_terms, num_trig, a, b);
        printf("Integral aproximada de %.2f a %.2f: %.2f\n", a, b, integral_val);
    } else {
        printf("Comando ou argumentos inválidos.\n");
        return 1;
    }
    
    return 0;
}
