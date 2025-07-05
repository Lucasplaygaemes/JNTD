// File: plugins/plugin_calc.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "plugin.h" // Your existing plugin header

// --- Missing Defines and Structs ---
#define MAX_TERMS 10
#define EPSILON 0.0001
#define TRAPEZOID_STEPS 1000

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


// --- Missing Helper Function Implementations ---

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
        if (strcmp(trig_terms[i].type, "sin") == 0) result += trig_terms[i].coef * sin(x);
        else if (strcmp(trig_terms[i].type, "cos") == 0) result += trig_terms[i].coef * cos(x);
        else if (strcmp(trig_terms[i].type, "tan") == 0) result += trig_terms[i].coef * tan(x);
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
            strcpy(result[i].type, "sec^2");
        }
    }
}

void print_polynomial(Term *terms, int num_terms) {
    int first = 1;
    for (int i = 0; i < num_terms; i++) {
        if (terms[i].coef == 0) continue;
        if (!first && terms[i].coef > 0) printf(" + ");
        else if (terms[i].coef < 0) printf(" - ");
        if (first && terms[i].coef < 0) printf("-");
        
        double abs_coef = fabs(terms[i].coef);
        if (abs_coef != 1 || terms[i].exp == 0) printf("%.2f", abs_coef);
        
        if (terms[i].exp > 0) {
            printf("x");
            if (terms[i].exp > 1) printf("^%d", terms[i].exp);
        }
        first = 0;
    }
    if (first) printf("0");
}

void print_trig(TrigTerm *trig_terms, int num_trig) {
    int first = 1;
    for (int i = 0; i < num_trig; i++) {
        if (trig_terms[i].coef == 0) continue;
        if (!first && trig_terms[i].coef > 0) printf(" + ");
        else if (trig_terms[i].coef < 0) printf(" - ");
        if (first && trig_terms[i].coef < 0) printf("-");
        
        double abs_coef = fabs(trig_terms[i].coef);
        if (abs_coef != 1) printf("%.2f", abs_coef);
        
        printf("%s(x)", trig_terms[i].type);
        first = 0;
    }
    if (first) printf("0");
}

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


// --- Corrected execute_calc Function ---
void execute_calc(const char *args) {
    if (args == NULL || strlen(args) == 0) {
        printf("Uso: calc <operacao> <parametros>...\n");
        return;
    }

    char buffer[512];
    // FIX: Use strncpy for safety to prevent buffer overflows
    strncpy(buffer, args, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char *argv[30];
    int argc = 0;

    argv[argc++] = "calc";
    char *token = strtok(buffer, " ");
    while(token != NULL && argc < 29) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    argv[argc] = NULL;

    // Check if there are enough arguments to avoid a crash
    if (argc < 2) {
        printf("Comando da calculadora invalido. Use 'calc <operacao>'.\n");
        return;
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
            printf("Erro: Divisao por zero!\n");
        }
    } else if (strcmp(argv[1], "deriv_poly") == 0 && argc >= 4 && (argc - 2) % 2 == 0) {
        int num_terms = (argc - 2) / 2;
        if (num_terms > MAX_TERMS) num_terms = MAX_TERMS;
        Term terms[MAX_TERMS] = {0};
        Term result[MAX_TERMS] = {0};
        
        for (int i = 0; i < num_terms; i++) {
            terms[i].coef = atof(argv[2 + 2 * i]);
            terms[i].exp = atoi(argv[3 + 2 * i]);
        }
        
        printf("Polinomio original: ");
        print_polynomial(terms, num_terms);
        printf("\n");
        derive_polynomial(terms, num_terms, result);
        printf("Derivada: ");
        print_polynomial(result, num_terms);
        printf("\n");
    } else if (strcmp(argv[1], "deriv_trig") == 0 && argc == 4) {
        TrigTerm trig_terms[1] = {0};
        TrigTerm result[1] = {0};
        trig_terms[0].coef = atof(argv[2]);
        strncpy(trig_terms[0].type, argv[3], sizeof(trig_terms[0].type) - 1);
        
        printf("Funcao original: ");
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
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "/") == 0) {
                is_trig = 1;
                continue;
            }
            if (!is_trig && i + 1 < argc && num_terms < MAX_TERMS) {
                terms[num_terms].coef = atof(argv[i]);
                terms[num_terms].exp = atoi(argv[i + 1]);
                num_terms++;
                i++; // Increment extra for the pair
            } else if (is_trig && i + 1 < argc && num_trig < MAX_TERMS) {
                trig_terms[num_trig].coef = atof(argv[i]);
                strncpy(trig_terms[num_trig].type, argv[i + 1], sizeof(trig_terms[num_trig].type) - 1);
                num_trig++;
                i++; // Increment extra for the pair
            }
        }
        double limit_val = calculate_limit(terms, num_terms, trig_terms, num_trig, a);
        printf("Limite aproximado em x = %.2f: %.4f\n", a, limit_val);
    } else if (strcmp(argv[1], "integ") == 0 && argc >= 5) {
        double a = atof(argv[2]);
        double b = atof(argv[3]);
        Term terms[MAX_TERMS] = {0};
        TrigTerm trig_terms[MAX_TERMS] = {0};
        int num_terms = 0, num_trig = 0;
        int is_trig = 0;
        for (int i = 4; i < argc; i++) {
            if (strcmp(argv[i], "/") == 0) {
                is_trig = 1;
                continue;
            }
            if (!is_trig && i + 1 < argc && num_terms < MAX_TERMS) {
                terms[num_terms].coef = atof(argv[i]);
                terms[num_terms].exp = atoi(argv[i + 1]);
                num_terms++;
                i++;
            } else if (is_trig && i + 1 < argc && num_trig < MAX_TERMS) {
                trig_terms[num_trig].coef = atof(argv[i]);
                strncpy(trig_terms[num_trig].type, argv[i + 1], sizeof(trig_terms[num_trig].type) - 1);
                num_trig++;
                i++;
            }
        }
        double integral_val = integrate_trapezoid(terms, num_terms, trig_terms, num_trig, a, b);
        printf("Integral aproximada de %.2f a %.2f: %.4f\n", a, b, integral_val);
    } else {
        printf("Comando ou argumentos invalidos para 'calc'.\n");
        // FIX: The function is void, so just return.
        return;
    }
}

// --- Correctly Placed register_plugin Function ---
Plugin* register_plugin() {
    static Plugin calc_plugin = {
        "calc",
        execute_calc
    };
    return &calc_plugin;
}