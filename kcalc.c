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
#include "kcalc_stack.h"

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

DEFINE_STACK(Token, Token)

static inline int precedence_comp(Token t, Token top, int t_is_unary,
				  int t_is_right_assoc);
int tokenize(const char *expr, const int expr_len, int *token_len,
	     int *paren_count, Token **result);
void print_tokens_debug(const Token *token_arr, const int token_len);
int shunting_yard(const Token *infix, const int token_len, int paren_count,
		  int *postfix_len, Token **result);

static inline int precedence(char op, int is_unary);

static inline int precedence(char op, int is_unary)
{
	// If we ever need more operators, it will be easier to add it.
	if (is_unary)
		return 30;

	switch (op) {
	case '+':
	case '-':
		return 10;
	case '*':
	case '%':
	case '/':
		return 20;
	case '^':
		return 40;
	default:
		return -1;
	}
}
static inline int precedence_comp(Token t, Token top, int t_is_unary,
				  int t_is_right_assoc)
{
	int top_is_unary = top.type == TOKEN_UNARY_MINUS ||
			   top.type == TOKEN_UNARY_PLUS;
	return precedence(t.op, top_is_unary) + t_is_right_assoc <=
	       precedence(top.op, top_is_unary);
}

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
	int postfix_len = 0;
	Token *token_arr = NULL, *postfix = NULL;

	pr_info("kcalc is loaded successfully.");
	pr_debug("Expression = %s", expr);
	expr_len = strlen(expr);

	if ((rc = tokenize(expr, expr_len, &token_len, &paren_count,
			   &token_arr)) != 0)
		goto err_tokenize;

	pr_debug("(token_len, paren_count) = (%d, %d)", token_len, paren_count);
	print_tokens_debug(token_arr, token_len);

	if ((rc = shunting_yard(token_arr, token_len, paren_count, &postfix_len,
				&postfix)) != 0) {
		goto err_shunting_yard;
	}

	pr_debug("postfix_len = %d", token_len);
	print_tokens_debug(postfix, postfix_len);

err_shunting_yard:
	kfree(postfix);
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
			pr_debug("Token #%d: (type, val) = (operator, '%c')", i,
				 t.op);
			break;
		case TOKEN_PAREN:
			pr_debug("Token #%d: (type, val) = (parenthesis, '%c')",
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

int shunting_yard(const Token *infix, const int tokens_len, int paren_count,
		  int *postfix_len, Token **result)
{
	int i = 0;
	int postfix_ptr = 0;
	int expect_opnd = 1;
	int rc = 0;
	Token *token_arr = NULL;
	Token *postfix = NULL;
	Token t;
	TokenStack st;

	token_arr = (Token *)kmalloc(sizeof(Token) * tokens_len, GFP_KERNEL);
	postfix = (Token *)kmalloc(sizeof(Token) * (tokens_len - paren_count),
				   GFP_KERNEL);

	if (!token_arr || !postfix) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	Token_stack_init(&st, token_arr, tokens_len);

	for (; i < tokens_len; ++i) {
		t = infix[i];
		switch (t.type) {
		case TOKEN_OPERAND: {
			postfix[postfix_ptr++] = t;
			expect_opnd = 0;
			break;
		}
		case TOKEN_OPERATOR: {
			int cur_uny = expect_opnd &&
				      (t.op == '+' || t.op == '-');
			Token *top;
			int is_r_assoc = t.op == '^' || cur_uny;

			if (cur_uny)
				t.type = ((t.op == '+') ? TOKEN_UNARY_PLUS :
							  TOKEN_UNARY_MINUS);

			// if is_right_assoc == 1 then we check precedence(...) < precedence(...)
			// if is_right_assoc == 0 then we check precedence(...) <= precedence(...)
			// We don't want to use tranary operators because that would require branching
			// Therefore we just add the is_right_assoc to the LHS, and only compare <=.

			while ((top = Token_stack_top(&st)) &&
			       top->type != TOKEN_PAREN &&
			       precedence_comp(t, *top, cur_uny, is_r_assoc)) {
				Token popped;
				// Already checked if top is NULL, so this will always return 0
				(void)Token_stack_pop(&st, &popped);
				postfix[postfix_ptr++] = popped;
			}

			if (Token_stack_push(&st, t) == -1) {
				rc = -EINVAL;
				goto err_inval;
			}

			expect_opnd = 1;
			break;
		}
		case TOKEN_PAREN: {
			if (t.op == '(') {
				if (Token_stack_push(&st, t) == -1) {
					rc = -EINVAL;
					goto err_inval;
				}
				expect_opnd = 1;
			} else {
				Token popped;
				int found_match = 0;
				while (Token_stack_pop(&st, &popped) == 0) {
					if (popped.type == TOKEN_PAREN &&
					    popped.op == '(') {
						found_match = 1;
						break;
					}
					postfix[postfix_ptr++] = popped;
				}
				if (!found_match) {
					rc = -EINVAL;
					goto err_inval;
				}

				expect_opnd = 0;
			}
			break;
		}
		case TOKEN_UNARY_MINUS:
		case TOKEN_UNARY_PLUS:
			break;
		default:
			goto err_inval;
		}
	}
	Token remaining;
	while (Token_stack_pop(&st, &remaining) == 0) {
		if (remaining.type == TOKEN_PAREN && remaining.op == '(') {
			// There is unmatched paraenthesis
			rc = -EINVAL;
			goto err_inval;
		}
		postfix[postfix_ptr++] = remaining;
	}

	*postfix_len = postfix_ptr;
	*result = postfix;

err_inval:
err_alloc:
	if (rc != 0)
		kfree(postfix);

	kfree(token_arr);
	return rc;
}
