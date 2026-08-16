#include <linux/errno.h>
#include <linux/gfp_types.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/overflow.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/string.h>
#include "kcalc.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMOGUS");
MODULE_DESCRIPTION("A calculator that runs directly in the kernel.");

static char *expr = "+";

MODULE_PARM_DESC(expr, "Expression");
module_param(expr, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

static int __init kcalc_init(void)
{
	int rc = 0;
	int expr_len = 0;
	int token_len = 0, paren_count = 0;
	int postfix_len = 0;
	long ans = 0;
	Token *tokens = NULL, *postfix = NULL;

	pr_info("kcalc is loaded successfully.");
	pr_debug("Expression = %s", expr);
	expr_len = strlen(expr);

	if ((rc = tokenize(expr, expr_len, &token_len, &paren_count,
			   &tokens)) != 0)
		goto err_tokenize;

	pr_debug("(token_len, paren_count) = (%d, %d)", token_len, paren_count);
	print_tokens_debug(tokens, token_len);

	if ((rc = shunting_yard(tokens, token_len, paren_count, &postfix_len,
				&postfix)) != 0)
		goto err_shunting_yard;

	pr_debug("postfix_len = %d", postfix_len);
	print_tokens_debug(postfix, postfix_len);

	if ((rc = eval(postfix, postfix_len, &ans)) != 0)
		goto err_eval;

	pr_info("Result = %ld", ans);

err_eval:
err_shunting_yard:
	kfree(postfix);
err_tokenize:
	kfree(tokens);
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
			pr_debug("Token #%d: (type, val) = (operand, %ld)", i,
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
