#include <linux/errno.h>
#include <linux/overflow.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include "kcalc.h"
#include "kcalc_stack.h"

DEFINE_STACK(Token, Token)

static inline int calc(char op, long a, long b, long *result)
{
	long res_local = 0;
	switch (op) {
	case '+':
		if (check_add_overflow(a, b, &res_local))
			goto err_overflow;
		break;
	case '-':
		if (check_sub_overflow(a, b, &res_local))
			goto err_overflow;
		break;
	case '*':
		if (check_mul_overflow(a, b, &res_local))
			goto err_overflow;
		break;
	case '/':
	case '%':
		if (b == 0) {
			pr_err("Division by zero.");
			return -EINVAL;
		}
		res_local = (op == '/') ? a / b : a % b;
		break;
	case '^':
		if (b < 0) {
			pr_err("Negative exponent not supported.");
			return -EINVAL;
		}
		{
			int i = 0;
			bool overflowed = false;

			res_local = 1;
			for (; i < b; ++i)
				overflowed |= check_mul_overflow(res_local, a,
								 &res_local);
			if (overflowed)
				goto err_overflow;
		}
		break;
	default:
		return -EINVAL;
	}
	*result = res_local;
	return 0;
err_overflow:
	pr_info("Result overflowed.");
	return -EOVERFLOW;
}

static inline int handle_unary_eval(Token t, TokenStack *st)
{
	Token x;
	if (Token_stack_pop(st, &x) == -1)
		return -EINVAL;
	if (t.type == TOKEN_UNARY_MINUS)
		x.num = -x.num;
	if (Token_stack_push(st, x) == -1)
		return -EINVAL;
	return 0;
}

static inline int handle_operator_eval(Token t, TokenStack *st)
{
	Token x, y;
	int rvx = Token_stack_pop(st, &x);
	int rvy = Token_stack_pop(st, &y);
	int rc = 0;
	if (rvx == -1 || rvy == -1)
		return -EINVAL;
	rc = calc(t.op, y.num, x.num, &(y.num));
	if (Token_stack_push(st, y) == -1)
		return -EINVAL;
	return rc;
}

int eval(Token *postfix, int len, long *result)
{
	TokenStack st;
	Token *token_stack_arr =
		(Token *)kmalloc(sizeof(Token) * len, GFP_KERNEL);
	Token t;
	int i = 0;
	int rc = 0;

	if (!token_stack_arr) {
		rc = -ENOMEM;
		goto err_alloc;
	}

	Token_stack_init(&st, token_stack_arr, len);

	for (; i < len; ++i) {
		t = postfix[i];
		if (t.type == TOKEN_OPERAND) {
			Token_stack_push(&st, t);
		} else if (t.type == TOKEN_OPERATOR) {
			rc = handle_operator_eval(t, &st);
		} else if (t.type == TOKEN_UNARY_MINUS ||
			   t.type == TOKEN_UNARY_PLUS) {
			rc = handle_unary_eval(t, &st);
		}
		if (rc != 0)
			goto err_inval;
	}
	if (Token_stack_empty(&st) || Token_stack_size(&st) > 1)
		goto err_inval;

	*result = Token_stack_top(&st)->num;
	Token_stack_free(&st);
	return 0;

err_inval:
err_alloc:
	*result = 0;
	kfree(token_stack_arr);
	return rc;
}
