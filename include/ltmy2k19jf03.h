/*
 * test-multiseg.c -- test the protocol for talking to the LTM-Y2K19JF-03
 *                    multi-segment display.
 *
 * Copyright 2015 Jeff Licquia.
 *
 * Header file for the low-level routines for writing to the display.
 *
 */

#include <stdint.h>

int ltm_display_init(int data_pin, int clock_pin, int reset_pin);
void ltm_display_shutdown();

void ltm_sleep(long usec);

int ltm_clear();

void ltm_blast_block(const uint8_t render_block[5]);

uint16_t ltm_find_alphanum_code(char c);
uint8_t ltm_find_numeric_code(char c);

void ltm_clear_alphanum(uint8_t block[5][5]);
void ltm_render_alphanum(const char *render, uint8_t block[5][5]);
void ltm_clear_numeric(uint8_t block[5][5]);
void ltm_render_numeric(const char *render, uint8_t block[5][5]);
