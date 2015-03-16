#!/usr/bin/make

#GPIO_IMPLEMENTATION = src/sysfs_gpio.c
GPIO_IMPLEMENTATION = src/wiringpi_gpio.c

EXTRA_LIBS = -lwiringPi

default: test_multiseg

test_multiseg: src/test_multiseg.c src/ltmy2k19jf03.c $(GPIO_IMPLEMENTATION)
	gcc -g -I include -o $@ $^ $(EXTRA_LIBS) -lpthread

clean:
	rm -f src/*.o test_multiseg
