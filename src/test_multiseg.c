/*
 * test-multiseg.c -- test the protocol for talking to the LTM-Y2K19JF-03
 *                    multi-segment display.
 *
 * Copyright 2015 Jeff Licquia.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include "gpio.h"
#include "ltmy2k19jf03.h"

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

    ltm_blast_block(local_block[current_block_counter]);

    /* Check for whether it's time to end. */

    pthread_testcancel();

    /* Set up for the next block to write. */

    current_block_counter++;
    if (current_block_counter > 4) {
      current_block_counter = 0;
    }

    /* Wait until the next opportunity to run. */

    ltm_sleep(700000);

    /* Time to grab an update. */

    if (++semaphore == 1) {
      for (i = 0; i < 5; i++) {
        for (j = 0; j < 5; j++) {
          local_block[i][j] = block[i][j];
        }
      }

      semaphore++;
    } else if (--semaphore < 0) {
      semaphore = 0;
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
  const char *letters[] =
    { "ABCDEFG",
      "HIJKLMN",
      "OPQRSTU",
      "VWXYZ",
      "0123456",
      "789",
      NULL };
  const char *numbers[] =
    { "0123",
      "456",
      "7890",
      NULL };

  /* Initialize the GPIO system. */

  check_error(gpio_init(), "couldn't initialize GPIO");

  check_error(ltm_display_init(GPIO_SEG_DATA, GPIO_SEG_CLOCK, GPIO_SEG_RESET),
	      "couldn't initialize I/O to device");

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

    while (semaphore < 2) {
      ltm_sleep(1);
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
      ltm_sleep(1);
    }

    /* Undo the previous writes in preparation for the next set. */

    for (i = 0; i < 5; i++) {
      block[i][segment_mask_index] = block[i][segment_mask_index] & (~segment_mask);
    }

    /* Wait. */

    sleep(1);
  }

  /* Now cycle through the known alphanumeric glyphs, rendering them
     on the first alphanumeric position. */

  j = 1;
  for (i = 0; letters[i] != NULL; i++) {
    while (semaphore < 2) {
      ltm_sleep(1);
    }

    fputs(letters[i], stdout);
    ltm_render_alphanum(letters[i], block);

    if (j && numbers[i] != NULL) {
      fputs(", ", stdout);
      fputs(numbers[i], stdout);
      ltm_render_numeric(numbers[i], block);
    } else if (numbers[i] == NULL) {
      j = 0;
      ltm_render_numeric("", block);
    };

    fputc('\n', stdout);

    semaphore = 0;

    sleep(5);
  }

  /* Stop the update thread. */

  pthread_cancel(blaster_thread);
  pthread_join(blaster_thread, NULL);

  /* Clean up display I/O and terminate. */

  ltm_display_shutdown();
  return 0;
}
