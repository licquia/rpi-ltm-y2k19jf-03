/*
 * gpio.h -- Manipulate GPIO pins.
 *
 * Copyright 2015 Jeff Licquia.
 *
 */

#define GPIO_DIR_INPUT 0
#define GPIO_DIR_OUTPUT 1
#define GPIO_PIN_LOW 0
#define GPIO_PIN_HIGH 1

int gpio_init();
int gpio_export_pin(int pin);
int gpio_unexport_pin(int pin);
int gpio_set_direction(int pin, int direction);
int gpio_write_pin(int pin, int setting);
