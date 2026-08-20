/*
 * chardev.c: Creates a read-only char device that says how many times
 * you have read from the dev file
 */

#include <linux/atomic.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h> /* for sprintf() */
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/uaccess.h> /* for get_user and put_user */
#include <linux/version.h>
#include "kcalc.h"

#include <asm/errno.h>

#define DEVICE_NAME "kcalc_chardev"
#define RESULT_BUF_SIZE 32
#define EXPR_BUF_SIZE 256

static int kcalc_device_open(struct inode *inode, struct file *file);
static int kcalc_device_release(struct inode *inode, struct file *file);
static ssize_t kcalc_device_read(struct file *filp, char __user *user_buf,
				 size_t len, loff_t *offset);
static ssize_t kcalc_device_write(struct file *filp,
				  const char __user *user_buf, size_t len,
				  loff_t *off);
/* Global variables are declared as static, so are global within the file. */

static size_t result_len;
static DEFINE_MUTEX(kcalc_buf_lock);
static int major;

static char result_buf[RESULT_BUF_SIZE];

static struct class *kcalc_cls;

static struct file_operations kcalc_chardev_fops = {
	.owner = THIS_MODULE,
	.read = kcalc_device_read,
	.write = kcalc_device_write,
	.open = kcalc_device_open,
	.release = kcalc_device_release,
};

int kcalc_chardev_init(void)
{
	int rc = 0;

	major = rc = register_chrdev(0, DEVICE_NAME, &kcalc_chardev_fops);

	if (rc < 0) {
		pr_alert("Registering char device failed with %d\n", major);
		goto err_register_dev;
	}

	pr_debug("Character device %s was assigned major number %d.\n",
		 DEVICE_NAME, major);

	kcalc_cls = class_create(DEVICE_NAME);
	if (IS_ERR(kcalc_cls)) {
		pr_err("Failed to create class for device\n");
		rc = PTR_ERR(kcalc_cls);
		goto err_class;
	}

	struct device *dev = device_create(kcalc_cls, NULL, MKDEV(major, 0),
					   NULL, DEVICE_NAME);
	if (IS_ERR(dev)) {
		rc = PTR_ERR(dev);
		pr_err("device_create failed: %d\n", rc);
		goto err_device_create; /* still need to unregister_chrdev */
	}

	pr_info("Device created on /dev/%s\n", DEVICE_NAME);
	return 0;

err_device_create:
	class_destroy(kcalc_cls);
err_class:
	unregister_chrdev(major, DEVICE_NAME);
err_register_dev:
	return rc;
}

void kcalc_chardev_exit(void)
{
	device_destroy(kcalc_cls, MKDEV(major, 0));
	class_destroy(kcalc_cls);

	/* Unregister the device */
	unregister_chrdev(major, DEVICE_NAME);
}

static int kcalc_device_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int kcalc_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t
kcalc_device_read(struct file *filp, /* see include/linux/fs.h   */
		  char __user *user_buf, /* buffer to fill with data */
		  size_t len, /* length of the buffer     */
		  loff_t *offset)
{
	ssize_t ret = 0;
	mutex_lock(&kcalc_buf_lock);
	if (*offset >= result_len) {
		ret = 0;
		goto out;
	}
	if (len > result_len - *offset)
		len = result_len - *offset;
	if (copy_to_user(user_buf, result_buf + *offset, len)) {
		ret = -EFAULT;
		goto out;
	}
	*offset += len;
	ret = len;
out:
	mutex_unlock(&kcalc_buf_lock);
	return ret;
}

static ssize_t kcalc_device_write(struct file *filp,
				  const char __user *user_buf, size_t len,
				  loff_t *off)
{
	int rc = 0;
	int expr_len = 0;
	int token_len = 0, paren_count = 0;
	int postfix_len = 0;
	long ans = 0;
	char *expr;
	Token *tokens = NULL, *postfix = NULL;

	pr_debug("/dev/%s is being written to.", DEVICE_NAME);

	if (len == 0) {
		rc = 0;
		goto empty_input;
	}

	if (len > EXPR_BUF_SIZE - 1) {
		rc = -EINVAL;
		goto err_bufwrite;
	}

	expr = (char *)kmalloc(len + 1, GFP_KERNEL);

	if (!expr) {
		rc = -ENOMEM;
		goto err_expr_alloc;
	}

	if (copy_from_user(expr, user_buf, len)) {
		rc = -EFAULT;
		goto err_copy_user_buf;
	}

	expr[len] = '\0';

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

	pr_debug("Result = %ld", ans);

	mutex_lock(&kcalc_buf_lock);
	result_len = scnprintf(result_buf, RESULT_BUF_SIZE, "%ld\n", ans);
	mutex_unlock(&kcalc_buf_lock);
	*off = 0;

err_eval:
err_shunting_yard:
	kfree(postfix);
err_tokenize:
	kfree(tokens);
err_copy_user_buf:
err_expr_alloc:
	kfree(expr);
err_bufwrite:
empty_input:
	return ((rc != 0) ? rc : (ssize_t)len);
}
