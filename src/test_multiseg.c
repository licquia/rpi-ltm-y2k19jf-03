/*
 * test-multiseg.c -- test the protocol for talking to the LTM-Y2K19JF-03
 *                    multi-segment display.
 *
 * Copyright 2015 Jeff Licquia.
 *
 * This routine tests writing to a generic multi-segment display which can
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
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

#include "gpio.h"

/* Global state variables, for thread communication. */

uint8_t semaphore = 2;

uint8_t block[5][5] = 
  { { 0x00, 0x00, 0x00, 0x04, 0x00 },
    { 0x00, 0x00, 0x00, 0x02, 0x00 },
    { 0x00, 0x00, 0x00, 0x01, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x80 },
    { 0x00, 0x00, 0x00, 0x00, 0x40 } };

/* Simple error handling. */

void check_error(const int retval, const char *errmsg)
{
  if (retval != 0) {
    fputs(errmsg, stderr);
    fputc('\n', stderr);
    exit(-1);
  }
}

#define GPIO_SEG_DATA 22
#define GPIO_SEG_CLOCK 17

#ifdef CONFIG_RASPI_REV_A
#define GPIO_SEG_RESET 21
#else
#define GPIO_SEG_RESET 27
#endif

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

void local_sleep(long usec)
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

  check_error(gpio_write_pin(GPIO_SEG_CLOCK, GPIO_PIN_LOW),
              "error setting clock pin low");

  check_error(gpio_write_pin(GPIO_SEG_DATA, bit_setting),
              "error setting data pin");
  local_sleep(1);
  check_error(gpio_write_pin(GPIO_SEG_CLOCK, GPIO_PIN_HIGH),
              "error setting clock pin high");
  local_sleep(1);
  check_error(gpio_write_pin(GPIO_SEG_CLOCK, GPIO_PIN_LOW),
              "error setting clock pin low");
}

/* Write an entire 34-byte block to the display controller. */

void blast_block(const uint8_t block[5])
{
  uint8_t local_block[5];
  int i, j;

  /* Mask off bits passed beyond 34.  This way, we can write the
     entire 40-byte block with the last 6 bits acting as stop bits for
     syncing purposes. */

  for (i = 0; i < 5; i++) {
    local_block[i] = block[i];
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

/* Main loop for actual writer thread.  This does the block switching
   thing, going through all five blocks so fast that they all appear
   lit.  We use a very simple global semaphore to control updates. */

void *blast_blocks_loop(void *arg)
{
  uint8_t local_block[5][5] = 
    { { 0x00, 0x00, 0x00, 0x04, 0x00 },
      { 0x00, 0x00, 0x00, 0x02, 0x00 },
      { 0x00, 0x00, 0x00, 0x01, 0x00 },
      { 0x00, 0x00, 0x00, 0x00, 0x80 },
      { 0x00, 0x00, 0x00, 0x00, 0x40 } };
  int current_block_counter = 0;
  int i, j;

  while (1) {
    /* Write the current block info. */

    blast_block(local_block[current_block_counter]);

    /* Check for whether it's time to end. */

    pthread_testcancel();

    /* Set up for the next block to write. */

    current_block_counter++;
    if (current_block_counter > 4) {
      current_block_counter = 0;
    }

    /* Wait until the next opportunity to run. */

    local_sleep(700000);

    /* Time to grab an update. */

    if (semaphore == 0) {
      semaphore++;

      for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
          local_block[i][j] = block[i][j];
        }
      }

      semaphore++;
    }
  }

  /* Warning suppression; we should never get here. */
  return NULL;
}

int main(int argc, char *argv)
{
  uint8_t segment_mask;
  int segment_mask_index, i, j;
  pthread_t blaster_thread;
  struct sched_param sched_p;

  /* Initialize the GPIO system. */

  check_error(gpio_init(), "couldn't initialize GPIO");

  gpio_export_pin(GPIO_SEG_DATA);
  gpio_set_direction(GPIO_SEG_DATA, GPIO_DIR_OUTPUT);
  gpio_export_pin(GPIO_SEG_CLOCK);
  gpio_set_direction(GPIO_SEG_CLOCK, GPIO_DIR_OUTPUT);
  gpio_export_pin(GPIO_SEG_RESET);
  gpio_set_direction(GPIO_SEG_RESET, GPIO_DIR_OUTPUT);

  /* Start the blaster loop. */

  check_error(pthread_create(&blaster_thread, NULL, blast_blocks_loop, NULL),
              "could not start thread");
  sched_p.sched_priority = 1;
  check_error(pthread_setschedparam(blaster_thread, SCHED_FIFO, &sched_p),
	      "could not set thread priority");

  /* Set up the next segments to light.  For now, that's one segment
     per group. */

  for (j = 0; j < 29; j++) {
    /* Wait for the semaphore to be ready. */

    while (semaphore == 1) {
      local_sleep(1);
    }

    /* Update the block structure with the current writes. */

    segment_mask_index = j / 8;
    segment_mask = 0x80 >> (j % 8);

    for (i = 0; i < 5; i++) {
      block[i][segment_mask_index] = block[i][segment_mask_index] | segment_mask;
    }

    /* Print the first block's values for monitoring purposes. */

    printf("%02X-%02X-%02X-%02X-%02X\n", block[0][0], block[0][1],
           block[0][2], block[0][3], block[0][4]);

    /* Signal the thread to update. */

    semaphore = 0;

    /* Wait for it to be read back. */

    while (semaphore < 2) {
      local_sleep(1);
    }

    /* Undo the previous writes in preparation for the next set. */

    for (i = 0; i < 5; i++) {
      block[i][segment_mask_index] = block[i][segment_mask_index] & (~segment_mask);
    }

    /* Wait 3 seconds. */

    sleep(3);
  }

  /* Stop the update thread. */

  pthread_cancel(blaster_thread);
  pthread_join(blaster_thread, NULL);

  /* Reset the display. */

  gpio_write_pin(GPIO_SEG_RESET, GPIO_PIN_HIGH);
  local_sleep(1);
  gpio_write_pin(GPIO_SEG_RESET, GPIO_PIN_LOW);

  /* Unregister the GPIO pins and terminate.  Note that we probably
     *should* unregister, but it seems to cause problems with
     *subsequent runs. */

  gpio_unexport_pin(GPIO_SEG_DATA);
  gpio_unexport_pin(GPIO_SEG_CLOCK);
  gpio_unexport_pin(GPIO_SEG_RESET);

  return 0;
}
