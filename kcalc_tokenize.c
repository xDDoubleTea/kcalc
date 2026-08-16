#include <linux/errno.h>
#include <linux/slab.h>
#include "kcalc.h"

int tokenize(const char *expr, const int expr_len, int *token_len,
	     int *paren_count, Token **result)
{
	int expr_p = 0, tokens_p = 0;
	long curr_num = 0;
	int state = 0;
	int parens = 0;
	int rc = 0;
	Token *tokens = (Token *)kmalloc(sizeof(Token) * expr_len, GFP_KERNEL);

	if (!tokens) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	for (; expr_p < expr_len; ++expr_p) {
		char ch = expr[expr_p];
		if ('0' <= ch && ch <= '9') {
			curr_num = curr_num * 10 + ch - '0';
			state = 1;
			continue;
		}
		if (state) {
			tokens[tokens_p].type = TOKEN_OPERAND;
			tokens[tokens_p].num = curr_num;
			tokens[tokens_p].op = '\0';
			curr_num = 0;
			state = 0;
			tokens_p++;
		}
		if (ch == ' ') {
			continue;
		} else if (ch == '+' || ch == '-' || ch == '*' || ch == '/' ||
			   ch == '%' || ch == '^') {
			tokens[tokens_p].type = TOKEN_OPERATOR;
			tokens[tokens_p].op = ch;
			tokens_p++;
		} else if (ch == '(' || ch == ')') {
			tokens[tokens_p].type = TOKEN_PAREN;
			tokens[tokens_p].op = ch;
			parens++;
			tokens_p++;
		} else {
			rc = -EINVAL;
			goto err_inval;
		}
	}
	if (state) {
		tokens[tokens_p].type = TOKEN_OPERAND;
		tokens[tokens_p].num = curr_num;
		tokens_p++;
	}

	if (parens % 2 != 0) {
		rc = -EINVAL;
		goto err_inval;
	}

	*token_len = tokens_p;
	*paren_count = parens;
	*result = tokens;
	pr_debug("(token_len, paren_count) = (%d, %d)", tokens_p, parens);
	return 0;

err_inval:
err_alloc:
	kfree(tokens);
	return rc;
}
