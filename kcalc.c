#include <linux/init.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/overflow.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>
#include <linux/types.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("AMOGUS");
MODULE_DESCRIPTION("A calculator that runs directly in the kernel.");

static long a = 0, b = 0;

static char *op = "+";

MODULE_PARM_DESC(a, "a");
module_param(a, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

MODULE_PARM_DESC(b, "b");
module_param(b, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

MODULE_PARM_DESC(op, "op");
module_param(op, charp, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

static inline int calc(char op, long a, long b)
{
	long result;
	switch (op) {
	case '+': {
		if (check_add_overflow(a, b, &result))
			pr_warn("Result overflowed.");
		pr_info("Result = %ld.", result);
	} break;
	case '-': {
		if (check_sub_overflow(a, b, &result))
			pr_warn("Result overflowed.");
		pr_info("Result = %ld", result);
	} break;
	case '*': {
		if (check_mul_overflow(a, b, &result))
			pr_warn("Result overflowed.");
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
			result = 1;
			int i;
			bool overflowed = false;

			// NOTE: 0^0 will be 1

			for (i = 0; i < b; ++i)
				overflowed |=
					check_mul_overflow(result, a, &result);
			if (overflowed)
				pr_err("Result overflowed.");
			pr_info("Result = %ld.", result);
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int __init kcalc_init(void)
{
	int i = 0;
	int rc = 0;

	pr_info("kcalc is loaded successfully.");
	pr_debug("(a, b) = (%ld, %ld).", a, b);

	while (op[i] != '\0')
		i++;

	pr_debug("oplen = %d.", i);
	if (i > 1 || i == 0)
		goto err_inval;

	pr_debug("Calculator mode %s.", op);

	rc = calc(op[0], a, b);
	return rc;
err_inval:
	return -EINVAL;
}

static void __exit kcalc_exit(void)
{
	pr_info("kcalc is unloaded successfully.");
}

module_init(kcalc_init);
module_exit(kcalc_exit);
