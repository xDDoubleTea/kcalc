obj-m += kcalc.o
kcalc-objs := kcalc_main.o kcalc_tokenize.o kcalc_shunting_yard.o kcalc_eval.o kcalc_chardev.o

all:
	echo $(PWD)
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	$(MAKE) -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
