#!/usr/bin/make

#GPIO_IMPLEMENTATION = src/sysfs_gpio.c
GPIO_IMPLEMENTATION = src/wiringpi_gpio.c

EXTRA_LIBS = -lwiringPi

LIB_SOURCES = src/ltmy2k19jf03.c $(GPIO_IMPLEMENTATION)

default: ltmy2kd test_multiseg

ltmy2kd: src/ltmy2kd.c $(LIB_SOURCES)
	gcc -g -I include -o $@ $^ $(EXTRA_LIBS)

test_multiseg: src/test_multiseg.c $(LIB_SOURCES)
	gcc -g -I include -o $@ $^ $(EXTRA_LIBS) -lpthread

clean:
	rm -f src/*.o test_multiseg ltmy2kd
