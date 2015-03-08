/*
 * sysfs_gpio.c -- Manipulate GPIO pins with sysfs.
 *
 * Copyright 2015 Jeff Licquia.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "gpio.h"

/*
 * Routines for controlling GPIO via sysfs.
 * Adapted from example code by Guillermo A. Amaral B. <g@maral.me>,
 * found at http://elinux.org/RPi_Low-level_peripherals.
 */

int gpio_export_pin(int pin)
{
  char buffer[3];
  ssize_t bytes;
  int fd;

  fd = open("/sys/class/gpio/export", O_WRONLY);
  if (fd == -1) {
    fputs("Failed to open export for writing!\n", stderr);
    return -1;
  }
 
  bytes = snprintf(buffer, 3, "%d", pin);
  write(fd, buffer, bytes);
  close(fd);
  return 0;
}

int gpio_unexport_pin(int pin)
{
  char buffer[3];
  ssize_t bytes;
  int fd;
 
  fd = open("/sys/class/gpio/unexport", O_WRONLY);
  if (fd == -1) {
    fputs("Failed to open unexport for writing!\n", stderr);
    return -1;
  }
 
  bytes = snprintf(buffer, 3, "%d", pin);
  write(fd, buffer, bytes);
  close(fd);
  return 0;
}

int gpio_set_direction(int pin, int direction)
{
  char *direction_str;
  int direction_length;
  char path[35];
  int fd;
 
  snprintf(path, 35, "/sys/class/gpio/gpio%d/direction", pin);
  fd = open(path, O_WRONLY);
  if (fd == -1) {
    fputs("Failed to open gpio direction for writing!\n", stderr);
    return -1;
  }

  if (direction == GPIO_DIR_INPUT) {
    direction_str = "in";
    direction_length = 2;
  } else if (direction == GPIO_DIR_OUTPUT) {
    direction_str = "out";
    direction_length = 3;
  } else {
    fputs("error: invalid direction\n", stderr);
    return -1;
  }
 
  if (write(fd, direction_str, direction_length) == -1) {
    fputs("Failed to set direction!\n", stderr);
    return -1;
  }
 
  close(fd);
  return 0;
}

int gpio_write_pin(int pin, int setting)
{
  static const char s_values_str[] = "01";
 
  char path[30];
  int fd;
  int values_index;
 
  snprintf(path, 30, "/sys/class/gpio/gpio%d/value", pin);
  fd = open(path, O_WRONLY);
  if (fd == -1) {
    fputs("Failed to open gpio value for writing!\n", stderr);
    return -1;
  }

  if (setting == GPIO_PIN_LOW) {
    values_index = 0;
  } else if (setting == GPIO_PIN_HIGH) {
    values_index = 1;
  } else {
    fputs("error: invalid pin setting value\n", stderr);
    return -1;
  }
 
  if (write(fd, &s_values_str[values_index], 1) != 1) {
    fprintf(stderr, "Failed to write value!\n");
    return -1;
  }
 
  close(fd);
  return 0;
}
