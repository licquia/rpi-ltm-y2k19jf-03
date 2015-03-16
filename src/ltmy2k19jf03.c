/*
 * ltmy2k19jf03.c -- low-level code for talking to the LTM-Y2K19JF-03
 *                   multi-segment display.
 *
 * Copyright 2015 Jeff Licquia.
 *
 * This code tests writing to a generic multi-segment display which can
 * be found occasionally in old parts bins.  Thanks to Marty Beem for
 * providing me one to play with.
 *
 * The part was reverse-engineered by David Cook.  Write-up here:
 *
 * http://www.robotroom.com/MultiSegmentLEDDisplay.html
 *
 * The basic idea is that the header on the back of this part takes
 * 5V signals like so:
 *
 *       data  X     X  +5VDC
 *      reset  X  X  X  ground
 *              clock
 *
 * Data is written in 36-byte blocks, with the first bit being a start
 * bit (always 1) and the last being a stop bit (which is ignored).
 * Use 0 for this bit and all non-specified bits; this allows for
 * resync if the two ends get out of sync.
 *
 * Writing a bit involves setting the data line low or high for 0 or
 * 1, respectively, waiting at least 300 nanoseconds, setting the
 * clock high, waiting at least 950 nanoseconds, and setting the clock
 * low.  Slower timings are perfectly fine.
 *
 * The bits are accumulated until all 36 have been written, at which
 * point the specified segments on the display light, and all
 * unspecified segments turn off.
 *
 * Note that there are 138 segments to light and only 34 usable bits.
 * This means that we have to cycle between 5 groups of segments.  The
 * last 5 bits before the stop bit indicate the group.  We therefore
 * have to continuously update the display, cycling through all 5
 * groups rapidly, in order for the entire display to be useful.
 * 
 * Here are the 5 groups, taken from the above page:
 *
 * 1) 1 start + 14 alpha + 1 ignore + 2 colon + 8 icon + 2 colon + 2
 *    ignore + 5 transistor + 1 zero = 36
 * 2) 1 start + 14 alpha + 7 numeric + 7 numeric + 1 ignore + 5
 *    transistor + 1 zero = 36
 * 3) 1 start + 14 alpha + 7 numeric + 7 numeric + 1 ignore + 5
 *    transistor + 1 zero = 36
 * 4) 1 start + 14 alpha + 14 alpha + 1 ignore + 5 transistor + 1 zero
 *    = 36
 * 5) 1 start + 14 alpha + 14 alpha + 1 ignore + 5 transistor + 1 zero
 *    = 36
 *
 * Writing an entire group of 5 requires delays totalling about
 * 225,000 nanoseconds, which means the whole update process should
 * take about a quarter of a millisecond.  This should give us plenty
 * of time for extra resync zero bits, and also not tax the processor
 * too heavily.
 *
 * This routine is written assuming the pins are connected to a
 * Raspberry Pi Model B, with the data pin hooked to GPIO 22, the
 * clock pin hooked to GPIO 17, and the reset pin hooked to GPIO 21/27
 * (depending on the version of the board).  I'm using a level shifter
 * from Adafruit to translate the 3.3V signals from the Pi to the 5V
 * signals the display expects (http://www.adafruit.com/products/757).
 */

#include <stdio.h>
#include <sys/time.h>
#include <errno.h>

#include "ltmy2k19jf03.h"
#include "gpio.h"

/* Globals for tracking pin state. */

int ltm_data_pin = 0;
int ltm_clock_pin = 0;
int ltm_reset_pin = 0;

/* For the 14-bit alphanumberic setups, create bit patterns for common
   characters to display. */

const uint16_t alphanum_chars[][2] = {
  { 'A', 0xEC88 },
  { 'B', 0xF2A0 },
  { 'C', 0x9C00 },
  { 'D', 0xF220 },
  { 'E', 0x9C88 },
  { 'F', 0x8C88 },
  { 'G', 0xBC80 },
  { 'H', 0x6C88 },
  { 'I', 0x9220 },
  { 'J', 0x7800 },
  { 'K', 0x0D48 },
  { 'L', 0x1C00 },
  { 'M', 0x6D04 },
  { 'N', 0x6C44 },
  { 'O', 0xFC00 },
  { 'P', 0xCC88 },
  { 'Q', 0xFC40 },
  { 'R', 0xCCC8 },
  { 'S', 0xB084 },
  { 'T', 0x8220 },
  { 'U', 0x7C00 },
  { 'V', 0x0D10 },
  { 'W', 0x6C50 },
  { 'X', 0x0154 },
  { 'Y', 0x0124 },
  { 'Z', 0x9110 },
  { '0', 0xFC00 },
  { '1', 0x6100 },
  { '2', 0xD888 },
  { '3', 0xF088 },
  { '4', 0x6488 },
  { '5', 0xB488 },
  { '6', 0xBC88 },
  { '7', 0xE000 },
  { '8', 0xFC88 },
  { '9', 0xF488 },
  { 0, 0 }
};

/* Do any setup needed to use the display. */

int ltm_display_init(int data_pin, int clock_pin, int reset_pin)
{
  gpio_export_pin(data_pin);
  gpio_set_direction(data_pin, GPIO_DIR_OUTPUT);
  gpio_export_pin(clock_pin);
  gpio_set_direction(clock_pin, GPIO_DIR_OUTPUT);
  gpio_export_pin(reset_pin);
  gpio_set_direction(reset_pin, GPIO_DIR_OUTPUT);

  ltm_data_pin = data_pin;
  ltm_clock_pin = clock_pin;
  ltm_reset_pin = reset_pin;

  return 0;
}

/* Shut down the display. */

void ltm_display_shutdown()
{
  /* Reset the display. */

  gpio_write_pin(ltm_reset_pin, GPIO_PIN_HIGH);
  ltm_sleep(1);
  gpio_write_pin(ltm_reset_pin, GPIO_PIN_LOW);

  /* Unregister the GPIO pins. */

  gpio_unexport_pin(ltm_data_pin);
  gpio_unexport_pin(ltm_clock_pin);
  gpio_unexport_pin(ltm_reset_pin);
}

/* Sleep for a specified number of microseconds.  For delays less
   than about 100 microseconds, just busy-wait; this seems to be best
   practice on Linux.  For longer times, nanosleep should do the
   trick.  See:

   https://projects.drogon.net/accurate-delays-on-the-raspberry-pi/

   Short loop busy-wait pulled from wiringPi.
 */

void delayMicrosecondsHard (unsigned int howLong)
{
  struct timeval tNow, tLong, tEnd ;

  gettimeofday (&tNow, NULL) ;
  tLong.tv_sec  = howLong / 1000000 ;
  tLong.tv_usec = howLong % 1000000 ;
  timeradd (&tNow, &tLong, &tEnd) ;

  while (timercmp (&tNow, &tEnd, <))
    gettimeofday (&tNow, NULL) ;
}

void ltm_sleep(long usec)
{
  struct timespec to_wait;
  struct timespec remaining;
  int sleep_retval;

  if (usec < 100) {
    delayMicrosecondsHard(usec);
  } else {
    to_wait.tv_sec = usec / 1000000;
    remaining.tv_sec = 0;
    to_wait.tv_nsec = usec % 1000000;
    remaining.tv_nsec = 0;
    sleep_retval = -1;
    errno = EINTR;
    while ((sleep_retval == -1) && (errno == EINTR)) {
      sleep_retval = nanosleep(&to_wait, &remaining);
      to_wait.tv_sec = remaining.tv_sec;
      to_wait.tv_nsec = remaining.tv_nsec;
    }
  }
}

/* Blast a single bit to the display controller.  The "bit" parameter is
   0 or 1.  It can be more than 1; we mask off all but the first bit
   for the sake of convenience. */

void blast_bit(const uint8_t bit)
{
  int bit_setting =
    ((bit & 0x01) == 0) ? GPIO_PIN_LOW : GPIO_PIN_HIGH;

  /* Failsafe: make sure the clock pin starts low every time. */

  gpio_write_pin(ltm_clock_pin, GPIO_PIN_LOW);
  gpio_write_pin(ltm_data_pin, bit_setting);

  ltm_sleep(1);
  gpio_write_pin(ltm_clock_pin, GPIO_PIN_HIGH);

  ltm_sleep(1);
  gpio_write_pin(ltm_clock_pin, GPIO_PIN_LOW);
}

/* Write an entire 34-byte block to the display controller. */

void ltm_blast_block(const uint8_t render_block[5])
{
  uint8_t local_block[5];
  int i, j;

  /* Mask off bits passed beyond 34.  This way, we can write the
     entire 40-byte block with the last 6 bits acting as stop bits for
     syncing purposes. */

  for (i = 0; i < 5; i++) {
    local_block[i] = render_block[i];
  }
  local_block[4] = local_block[4] & 0xc0;

  /* Start bit. */

  blast_bit(1);

  /* Now go through the entire block bit by bit. */

  for (i = 0; i < 5; i++) {
    for (j = 7; j >= 0; j--) {
      blast_bit(local_block[i] >> j);
    }
  }
}

/* For a given character, return the bit code to render the character
   on one of the alphanumberic spaces. */

uint16_t ltm_find_alphanum_code(char c)
{
  uint16_t code;
  int i;

  /* Asterisk - the default code, used for 'not found'. */

  code = 0x03FC; 

  /* Find the code for this character. */
  for (i = 0; alphanum_chars[i][0] != 0; i++) {
    if (((char)alphanum_chars[i][0]) == c) {
      code = alphanum_chars[i][1];
      break;
    }
  }

  return code;
}

/* Zero out the alphanum sections. */

void ltm_clear_alphanum(uint8_t block[5][5])
{
  int i;

  for (i = 0; i < 5; i++) {
    block[i][0] = 0;
    block[i][1] = block[i][1] & 0x02;
  }
  for (i = 3; i < 5; i++) {
    block[i][1] = 0;
    block[i][2] = 0;
    block[i][3] = block[i][3] & 0x0F;
  }
}

/* Render the string into the alphanum section of the display. */

void ltm_render_alphanum(const char *render, uint8_t block[5][5])
{
  int i, j;
  uint16_t code;

  ltm_clear_alphanum(block);

  for (i = 0; i < 7 && render[i] != '\0'; i++) {
    code = ltm_find_alphanum_code(render[i]);

    if (i < 5) {
      block[i][0] = block[i][0] | (uint8_t)((code & 0xFF00) >> 8);
      block[i][1] = block[i][1] | (uint8_t)(code & 0x00FC);
    } else {
      j = i - 2;
      block[j][1] = block[j][1] | (uint8_t)((code & 0xC000) >> 14);
      block[j][2] = block[j][2] | (uint8_t)((code & 0x3FC0) >> 6);
      block[j][3] = block[j][3] | (uint8_t)((code & 0x003C) << 2);
    }
  }
}

/* Convert the alphanum rendering of a digit character to numeric
   encoding, suitable for the numeric display at the bottom. */

uint8_t ltm_find_numeric_code(char c)
{
  uint8_t middle_seg = 0;
  uint16_t code = 0x03FC; 

  if ((c >= '0') && (c <= '9')) {
    code = ltm_find_alphanum_code(c);
  }

  if ((code & 0x0088) != 0) {
    middle_seg = 2;
  }

  return ((uint8_t)((code & 0xFC00) >> 8)) | middle_seg;
}

/* Clear out the numeric sections. */

void ltm_clear_numeric(uint8_t block[5][5])
{
  int i;

  for (i = 1; i < 3; i++) {
    block[i][1] = block[i][1] & 0xFC;
    block[i][2] = 0;
    block[i][3] = block[i][3] & 0x0F;
  }
}

/* Render the given string into the numeric section of the display. */

void ltm_render_numeric(const char *render, uint8_t block[5][5])
{
  uint8_t code;
  int i, block_index;

  ltm_clear_numeric(block);

  for (i = 0; i < 4 && render[i] != '\0'; i++) {
    code = ltm_find_numeric_code(render[i]);

    if ((i % 2) == 0) {
      block_index = 1;
    } else {
      block_index = 2;
    }

    if (i < 2) {
      block[block_index][1] = block[block_index][1] | ((code & 0xC0) >> 6);
      block[block_index][2] = block[block_index][2] | ((code & 0x3E) << 2);
    } else {
      block[block_index][2] = block[block_index][2] | ((code & 0xE0) >> 5);
      block[block_index][3] = block[block_index][3] | ((code & 0x1E) << 3);
    }
  }
}
