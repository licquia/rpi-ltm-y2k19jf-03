#!/usr/bin/make

#GPIO_IMPLEMENTATION = src/sysfs_gpio.o
GPIO_IMPLEMENTATION = src/wiringpi_gpio.o

EXTRA_LIBS = -lwiringPi

LIB_OBJFILES = src/ltmy2k19jf03.o $(GPIO_IMPLEMENTATION)

CFLAGS = -g -Iinclude
LDLIBS = $(EXTRA_LIBS)

default: ltmy2kd test_multiseg

ltmy2kd: src/ltmy2kd.o $(LIB_OBJFILES)
	$(CC) -o $@ $^ $(LDLIBS)

test_multiseg: src/test_multiseg.o $(LIB_OBJFILES)
	$(CC) -o $@ $^ $(LDLIBS) -lpthread

clean:
	rm -f src/*.o test_multiseg ltmy2kd
