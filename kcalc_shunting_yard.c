#include <linux/errno.h>
#include <linux/slab.h>
#include "kcalc.h"
#include "kcalc_stack.h"

DEFINE_STACK(Token, Token)

static inline int precedence(char op, int is_unary)
{
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
	int top_is_unary = ((top.type == TOKEN_UNARY_MINUS) ||
			    (top.type == TOKEN_UNARY_PLUS));
	return precedence(t.op, t_is_unary) + t_is_right_assoc <=
	       precedence(top.op, top_is_unary);
}

static inline int handle_operand(Token t, Token *postfix, int *postfix_ptr,
				 int *expect_opnd)
{
	postfix[(*postfix_ptr)++] = t;
	*expect_opnd = 0;
	return 0;
}

static inline int handle_operator(Token t, TokenStack *st, Token *postfix,
				  int *postfix_ptr, int *expect_opnd)
{
	int cur_uny = ((*expect_opnd == 1) && (t.op == '+' || t.op == '-'));
	int is_r_assoc = ((t.op == '^') || cur_uny);
	Token *top;

	if (cur_uny)
		t.type = (t.op == '+') ? TOKEN_UNARY_PLUS : TOKEN_UNARY_MINUS;

	while ((top = Token_stack_top(st)) && top->type != TOKEN_PAREN &&
	       precedence_comp(t, *top, cur_uny, is_r_assoc)) {
		Token popped;
		(void)Token_stack_pop(st, &popped);
		postfix[(*postfix_ptr)++] = popped;
	}

	if (Token_stack_push(st, t) == -1)
		return -EINVAL;

	*expect_opnd = 1;
	return 0;
}

static inline int handle_paren(Token t, TokenStack *st, Token *postfix,
			       int *postfix_ptr, int *expect_opnd)
{
	if (t.op == '(') {
		if (Token_stack_push(st, t) == -1)
			return -EINVAL;
		*expect_opnd = 1;
		return 0;
	}

	Token popped;
	int found_match = 0;

	while (Token_stack_pop(st, &popped) == 0) {
		if (popped.type == TOKEN_PAREN && popped.op == '(') {
			found_match = 1;
			break;
		}
		postfix[(*postfix_ptr)++] = popped;
	}
	if (!found_match)
		return -EINVAL;

	*expect_opnd = 0;
	return 0;
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
		case TOKEN_OPERAND:
			rc = handle_operand(t, postfix, &postfix_ptr,
					    &expect_opnd);
			break;
		case TOKEN_OPERATOR:
			rc = handle_operator(t, &st, postfix, &postfix_ptr,
					     &expect_opnd);
			break;
		case TOKEN_PAREN:
			rc = handle_paren(t, &st, postfix, &postfix_ptr,
					  &expect_opnd);
			break;
		case TOKEN_UNARY_MINUS:
		case TOKEN_UNARY_PLUS:
			rc = 0;
			break;
		default:
			rc = -EINVAL;
			goto err_inval;
		}
		if (rc != 0)
			goto err_inval;
	}
	Token remaining;
	while (Token_stack_pop(&st, &remaining) == 0) {
		if (remaining.type == TOKEN_PAREN && remaining.op == '(') {
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
