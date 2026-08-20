#ifndef KCALC_H
#define KCALC_H

typedef enum {
	TOKEN_OPERATOR,
	TOKEN_OPERAND,
	TOKEN_PAREN,
	TOKEN_UNARY_MINUS,
	TOKEN_UNARY_PLUS
} token_t;

typedef struct _TOKEN {
	long num;
	char op;
	token_t type;
} Token;

int tokenize(const char *expr, const int expr_len, int *token_len,
	     int *paren_count, Token **result);
int shunting_yard(const Token *infix, const int token_len, int paren_count,
		  int *postfix_len, Token **result);
int eval(Token *postfix, int len, long *result);
void print_tokens_debug(const Token *token_arr, const int token_len);

int kcalc_chardev_init(void);
void kcalc_chardev_exit(void);

#endif /* KCALC_H */
