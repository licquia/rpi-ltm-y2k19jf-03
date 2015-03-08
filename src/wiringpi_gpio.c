/*
 * wiringpi_gpio.c -- Manipulate GPIO pins with wiringPi.
 *
 * Copyright 2015 Jeff Licquia.
 *
 */

#include <stdio.h>
#include <wiringPi.h>

#include "gpio.h"

/*
 * Routines for controlling GPIO via wiringPi.
 * Adapted from example code by Gordon Henderson, projects@drogon.net,
 * found at http://elinux.org/RPi_Low-level_peripherals.
 */

int gpio_init()
{
  return wiringPiSetupGpio();
}

int gpio_export_pin(int pin)
{
  return 0;
}

int gpio_unexport_pin(int pin)
{
  return 0;
}

int gpio_set_direction(int pin, int direction)
{
  if (direction == GPIO_DIR_INPUT) {
    pinMode(pin, INPUT);
  } else if (direction == GPIO_DIR_OUTPUT) {
    pinMode(pin, OUTPUT);
  } else {
    fputs("error: invalid direction\n", stderr);
    return -1;
  }

  return 0;
}

int gpio_write_pin(int pin, int setting)
{
  digitalWrite(pin, setting);
  return 0;
}
