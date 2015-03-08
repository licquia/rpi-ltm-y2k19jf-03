#!/usr/bin/make

default: test_multiseg

test_multiseg: src/test_multiseg.c src/sysfs_gpio.c
	gcc -I include -o $@ $^ -lpthread

clean:
	rm -f src/*.o test_multiseg
