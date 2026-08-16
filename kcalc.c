#include "asm-generic/errno.h"
#include "linux/gfp_types.h"
#include "linux/printk.h"
#include "linux/slab.h"
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/overflow.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMOGUS");
MODULE_DESCRIPTION("A calculator that runs directly in the kernel.");

static char *expr = "+";

MODULE_PARM_DESC(expr, "Expression");
module_param(expr, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

typedef enum {
	TOKEN_OPERATOR,
	TOKEN_OPERAND,
	TOKEN_PAREN,
	TOKEN_UNARY_MINUS,
	TOKEN_UNARY_PLUS
} token_t;

typedef struct _TOKEN {
	int num;
	char op;
	token_t type;
} Token;

int tokenize(const char *, const int, int *, int *, Token **);
void print_tokens_debug(const Token *, const int);

static inline int calc(char op, long a, long b)
{
	long result;
	switch (op) {
	case '+': {
		if (check_add_overflow(a, b, &result))
			goto err_overflow;
		pr_info("Result = %ld.", result);
	} break;
	case '-': {
		if (check_sub_overflow(a, b, &result))
			goto err_overflow;
		pr_info("Result = %ld", result);
	} break;
	case '*': {
		if (check_mul_overflow(a, b, &result))
			goto err_overflow;
		pr_info("Result = %ld", result);
	} break;
	case '/':
	case '%':
		if (b == 0) {
			pr_err("Division by zero.");
			return -EINVAL;
		}
		if (op == '/')
			pr_info("Result = %ld.", a / b);
		else
			pr_info("Result = %ld.", a % b);
		break;
	case '^':
		if (b < 0) {
			pr_err("Negative exponent not supported.");
			return -EINVAL;
		}
		{
			int i = 0;
			bool overflowed = false;

			// NOTE: 0^0 will be 1

			result = 1;
			for (; i < b; ++i)
				overflowed |=
					check_mul_overflow(result, a, &result);
			if (overflowed)
				goto err_overflow;
			pr_info("Result = %ld.", result);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
err_overflow:
	pr_info("Result overflowed.");
	return -EOVERFLOW;
}

static int __init kcalc_init(void)
{
	int rc = 0;
	int expr_len = 0;
	int token_len = 0, paren_count = 0;
	Token *token_arr = NULL;

	pr_info("kcalc is loaded successfully.");
	pr_debug("Expression = %s", expr);
	expr_len = strlen(expr);

	rc = tokenize(expr, expr_len, &token_len, &paren_count, &token_arr);
	pr_info("(token_len, paren_count) = (%d, %d)", token_len, paren_count);
	print_tokens_debug(token_arr, token_len);
	if (rc != 0)
		goto err_tokenize;
err_tokenize:
	kfree(token_arr);
	return rc;
}

static void __exit kcalc_exit(void)
{
	pr_info("kcalc is unloaded successfully.");
}

module_init(kcalc_init);
module_exit(kcalc_exit);

void print_tokens_debug(const Token *token_arr, const int token_len)
{
	int i = 0;
	Token t;

	pr_debug("----\n");
	for (; i < token_len; ++i) {
		t = token_arr[i];
		switch (t.type) {
		case TOKEN_OPERAND:
			pr_debug("Token #%d: (type, val) = (operand, %d)", i,
				 t.num);
			break;
		case TOKEN_OPERATOR:
			pr_debug("Token #%d: (type, val) = (opeator, '%c')", i,
				 t.op);
			break;
		case TOKEN_PAREN:
			pr_debug(
				"Token #%d: (type, val) = (paraenthesis, '%c')",
				i, t.op);
			break;
		case TOKEN_UNARY_MINUS:
			pr_debug("Token #%d: (type, val) = (unary_minus, '%c')",
				 i, t.op);
			break;
		case TOKEN_UNARY_PLUS:
			pr_debug("Token #%d: (type, val) = (unary_plus, '%c')",
				 i, t.op);
			break;
		}
	}
	pr_debug("----\n");
}

int tokenize(const char *expr, const int expr_len, int *token_len,
	     int *paren_count, Token **result)
{
	int expr_p = 0, tokens_p = 0;
	int curr_num = 0;
	int state = 0;
	int parens = 0;
	int rc = 0;
	Token *tokens = (Token *)kmalloc(sizeof(Token) * expr_len, GFP_KERNEL);

	if (!tokens) {
		rc = -1;
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

	// If parens is odd then the parenthese must not be closed properly
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
