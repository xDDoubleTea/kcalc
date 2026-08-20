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
MODULE_AUTHOR("404");
MODULE_DESCRIPTION("A calculator that runs directly in the kernel.");

static char *expr = "+";

MODULE_PARM_DESC(expr, "Expression");
module_param(expr, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

static int __init kcalc_init(void)
{
	pr_info("kcalc is loading...\n");
	return kcalc_chardev_init();
}

static void __exit kcalc_exit(void)
{
	kcalc_chardev_exit();
	pr_info("kcalc is unloaded successfully.\n");
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
