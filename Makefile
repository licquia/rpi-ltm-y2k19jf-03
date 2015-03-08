#!/usr/bin/make

GPIO_IMPLEMENTATION = src/sysfs_gpio.c

default: test_multiseg

test_multiseg: src/test_multiseg.c $(GPIO_IMPLEMENTATION)
	gcc -I include -o $@ $^ $(EXTRA_LIBS) -lpthread

clean:
	rm -f src/*.o test_multiseg
