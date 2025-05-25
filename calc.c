#include <stdio.h>
#include <math.h>

int main() {
	char oper;
	float num1, num2, resultado;
	char conti;

	do {
		system("cls");
		
		printf("=== Calculadora Simples ===\n");
		printf("operações disponiveis são mostradas com "help"\n");
		printf("\nDigite Operação Desejadas: ");
		scanf(" %c", &operacao);

		int op_un = 0;
		switch (operacao) {
			case 's': case 'c': case 'r': case 'q': op_un = 1; break;
		}

		if(op_un) {
			printf("Digite o numero: ");
			scanf("%f", &num1);

		} else {
			printf("Digite o primeiro Numero: ");
			scanf("%f", &num1);

			if(operacao == '+' || operacao == '-' || operacao == '*' || operacao == '/' || operacao 'p') {
				printf("digite o segundo numero: ");
				scanf("%f", &num2);
			}
		
		}
		switch(operacao) {
			case '+':
				resultado = num1 + num2;
				printf("\nResultado: %.2f + %.2f = %.2f\n", num1, num2, result);
				break;

			case '-':
				resultado = num1 - num2;
				printf("\nResultado: %.2f - %.2f = %.2f\n", num1, num2, resultado);
				break;
			case '*':
				resultado = num1 * num2;
				printf("\nResultado: %.2f * %.2f = %.2f\n", num1, num2, resultado);
				break;
			case '/':
				if(num2 != 0) {
					resultado = num1 / num2;
					printf("\nResultado: %.2f / %.2f = %.2f\n", num1, num2, resultado);
				} else {
					printf("\nErro: Divisao por zero!\n");
				}
				break;
			case 's':
				resultado = sin(num1);
				printf("\nResultado: sin(%.2f) = %.2f\n", num1, resultado
