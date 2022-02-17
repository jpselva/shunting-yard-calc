#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#define MAX_TK_SZ 100         /* maximum size of an input token string (includes \0) */
#define NUM_STACK_SZ 100
#define SYMBOL_STACK_SZ 100 

/* types for holding information about tokens */ 
typedef enum { NUMBER = 0, LPAREN, RPAREN, OPERATOR, 
               EOLINE, EOFILE, UNKNOWN } kind_of_token;
typedef struct {
	kind_of_token type;
	union {
		double number;
		char symbol;
	};
} token;

int read_token(token *);
bool known_operation(char);
bool should_pop(char, char);
int apply(double, double, char, double*);
int priority(char);
void report_error(const char*, ...);

int main(void)
{
	double nstk[NUM_STACK_SZ];    /* number stack */
	char   sstk[SYMBOL_STACK_SZ]; /* symbol stack */
	int    nsp = 0,               /* number stack 'pointer' */
		   ssp = 0;               /* symbol stack 'pointer' */
	bool   should_exit;
	char   c;
	double op1, op2;
	token tk;
	bool want_number;

	nsp = ssp = 0;
	should_exit = false;
	want_number = 1;
	printf("\n> ");

	while (!should_exit) {
		if (read_token(&tk))
			goto recover;

		if (want_number) {
			switch(tk.type) {
			case NUMBER:
				if (nsp == NUM_STACK_SZ) {
					report_error("number stack overflowed");
					goto recover;
				}
				nstk[nsp++] = tk.number;
				want_number = 0;
				break;
			case LPAREN:
				if (ssp == SYMBOL_STACK_SZ) {
					report_error("symbol stack overflowed");
					goto recover;
				}
				sstk[ssp++] = tk.symbol;
				want_number = 1;
				break;
			case OPERATOR: /* unary + or - */
				if (tk.symbol == '-') {
					/* turn -x into 0 - x */
					if (nsp == NUM_STACK_SZ) {
						report_error("number stack overflowed");
						goto recover;
					}
					nstk[nsp++] = 0;

					if (ssp == SYMBOL_STACK_SZ) {
						report_error("symbol stack overflowed");
						goto recover;
					}
					sstk[ssp++] = '-';
				} else if (tk.symbol != '+') {
					report_error("%c is not an unary operator", tk.symbol);
					goto recover;
				}
				break;
			case EOFILE:
				should_exit = true;
				break;
			default:
				report_error("expected number or '('");
				goto recover;
				break;
			}
		} else { /* not looking for number */
			if (tk.type != OPERATOR && tk.type != RPAREN && tk.type != EOLINE) {
				report_error("expected operator, ')' or newline");
				goto recover;
			}

			/* pop operators with higher priority from sstk. In terms
			 * of priority, only '(' is below ')'. */
			while (ssp > 0 && should_pop(sstk[ssp - 1], tk.symbol)) {
				if (nsp < 2) {
					report_error("number stack underflowed");
					goto recover;
				}

				op2 = nstk[--nsp];
				op1 = nstk[--nsp];

				if (apply(op1, op2, sstk[--ssp], &nstk[nsp++]))
					goto recover;
			}

			if (known_operation(tk.symbol)) {
				/* push operator unconditionally */
				if (ssp == SYMBOL_STACK_SZ) {
					report_error("symbol stack overflowed");
					goto recover;
				}

				sstk[ssp++] = tk.symbol;
				want_number = 1;
			} else if (tk.type == RPAREN) {
				/* make sure top of sstk is '(' and pop */
				if (ssp < 1) {
					report_error("unmatched ')'");
					goto recover;
				}
				ssp -= 1;
				want_number = 0;
			} else if (tk.type == EOLINE) {
				if (nsp == 1) {
					printf("%f\n", nstk[--nsp]);
				} else if (nsp != 0) {
					/* nsp != 0 because last expression could have returned
					   nothing and that wouldn't be an error */
					report_error("number stack ended with > 1 number");
					goto recover;
				}

				want_number = 1;
				printf("\n> ");
			}
		}

		continue;
		recover:
		if (tk.type != EOLINE) /* if there is remaining input */
			while ((c = getchar()) != '\n' && c != EOF) /* flush input buffer */
				;

		ssp = nsp = 0;
		want_number = false;
		ungetc('\n', stdin); /* force '>' prompt */
	}

	putchar('\n'); /* ctrl-d does not add new line for shell prompt */

	return 0;
}

/* peek at next char in stdin */
int peek()
{
	int c = getchar();
	if (c != EOF)
		ungetc(c, stdin);
	return c;
}

/* reads a token into tk, returns error status */
int read_token(token* tk)
{
	char num_string[MAX_TK_SZ];  /* digit string */
	int  num_len,                /* size of digit string */
	     lc;                     /* last char read */

	while ((lc = getchar()) == ' ' || lc == '\t')
		;

	if (!isdigit(lc) && !(lc == '.' && isdigit(peek()))) { /* not a number */
		switch (lc) {
			case '\n':
				tk->type = EOLINE;
				break;
			case EOF:
				tk->type = EOFILE;
				break;
			case '(':
				tk->type = LPAREN;
				break;
			case ')':
				tk->type = RPAREN;
				break;
			default:
				if (known_operation(lc))
					tk->type = OPERATOR;
				else
					tk->type = UNKNOWN;
				break;
		};
		tk->symbol = lc;
		return 0;
	} /* otherwise, attempt to read a number */

	for (num_len = 0; isdigit(lc); lc = getchar()) {
		if (num_len >= MAX_TK_SZ) {
			report_error("max token size exceeded");
			return 1;
		}
		num_string[num_len++] = lc;
	}

	if (lc == '.' && isdigit(peek())) { /* found decimal part? */
		do {
			if (num_len >= MAX_TK_SZ) {
				report_error("max token size exceeded");
				return 1;
			}
			num_string[num_len++] = lc;
		} while (isdigit(lc = getchar()));
	}

	num_string[num_len] = '\0'; /* the last char read is part of the next token */
	if (lc != EOF)
		ungetc(lc, stdin);	

	tk->type = NUMBER;
	tk->number = atof(num_string);
	return 0;
}

/* Is 'op' a supported arithmetic operation? */
bool known_operation(char op)
{
	return op == '+' || op == '-' || op == '*' || op == '/';
}

/* Returns priority of symbol 'op' */ 
int priority(char op)
{
	switch (op) {
	case '\n':
		return -1;
	case '(':
		return 0;
	case ')':
		return 1;
	case '+':
	case '-':
		return 2;
	case '*':
	case '/':
		return 3;
	default:
		return -1;
	}
}

/* Takes top of stack operator and new operator to be pushed.
   Returns true iff sstk has to be popped */
bool should_pop(char tos, char new_op)
{
	return (priority(tos) > priority(new_op)) ||
		   ((priority(tos) == priority(new_op)) &&
	        (tos == '-' || tos == '/'));
}

/* apply 'operation' to 'op1' and 'op2', store in 'result'. Return error
 * status */
int apply(double op1, double op2, char operation, double *result)
{
	switch (operation) {
	case '+':
		*result = op1 + op2;
		break;
	case '-':
		*result = op1 - op2;
		break;
	case '*':
		*result = op1 * op2;
		break;
	case '/':
		if (op2 == 0) {
			report_error("division by 0");
			return 1;
		}
		*result = op1 / op2;
		break;
	default:
		break;
	}

	return 0;
}

void report_error(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	fprintf(stderr, "ERROR: ");
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
}
