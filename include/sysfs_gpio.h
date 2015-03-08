/*
 * sysfs_gpio.h -- Manipulate GPIO pins with sysfs.
 *
 * Copyright 2015 Jeff Licquia.
 *
 */

#define SYSFS_GPIO_DIR_INPUT 0
#define SYSFS_GPIO_DIR_OUTPUT 1
#define SYSFS_GPIO_PIN_LOW 0
#define SYSFS_GPIO_PIN_HIGH 1

int sysfs_gpio_export_pin(int pin);
int sysfs_gpio_unexport_pin(int pin);
int sysfs_gpio_set_direction(int pin, int direction);
int sysfs_gpio_write_pin(int pin, int setting);
