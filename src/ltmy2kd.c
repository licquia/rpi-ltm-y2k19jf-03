/*
 * ltmy2kd.c -- daemon for managing the LTM-Y2K19JF-03 multi-segment display.
 *
 * Copyright 2015 Jeff Licquia.
 *
 * This daemon manages the LTM-Y2K19JF-03 display, a low-level
 * multi-segment display sometimes found in old parts bins.
 *
 * The display requires constant updating, as it uses multiplexing to
 * handle an internal chip limitation.  Only a small portion of the
 * display can be lit at one time; therefore, the host must write new
 * displays fast enough to provide the illusion of constancy.
 *
 * The daemon receives commands via a pipe in /run/ltmy2kd.
 * Currently, only the following commands are supported:
 *
 * ALPHA string   display the string on the alphanum-capable portion
 *                of the display.  Limited to 7 characters (extras are
 *                just dropped).  Only uppercase letters and numbers
 *                are supported; anything else is replaced with a '*'.
 * NUM string     display the string on the numeric-capable portion of
 *                the display.  Limited to 4 characters (extras are
 *                just dropped).  Only numbers are supported; anything
 *                else is replaced with a '-'.
 *
 * The display also supports colons in two places (with each dot
 * indivudually addressable) and four icons, so this list of commands
 * may grow.  Each command may pass no string, which blanks out the
 * display.
 *
 * The code assumes a Raspberry Pi GPIO setup, with certain pins
 * defined as the data, clock, and reset pins.  Changing these will
 * require editing the source code and rebuilding.  A patch to make
 * this configurable would be welcome.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <string.h>

#include "ltmy2k19jf03.h"

/* GPIO pins to control the display. */

#define GPIO_SEG_DATA 22
#define GPIO_SEG_CLOCK 17

#ifdef CONFIG_RASPI_REV_A
#define GPIO_SEG_RESET 21
#else
#define GPIO_SEG_RESET 27
#endif

/* Named pipe to use for receiving commands. */

#define CMD_PATH "/run/ltmy2kd"

/* Timeouts for polling.  We set a long timeout when the display is
   blank, because we don't have to really do anything in that case. */

#define POLL_TIMEOUT_BLANK 5000
#define POLL_TIMEOUT_DATA 500

/* Global state. */

char alphanum_string[8] = "";
char numeric_string[5] = "";

uint8_t block[5][5] = 
  { { 0x00, 0x00, 0x00, 0x04, 0x00 },
    { 0x00, 0x00, 0x00, 0x02, 0x00 },
    { 0x00, 0x00, 0x00, 0x01, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x80 },
    { 0x00, 0x00, 0x00, 0x00, 0x40 } };

void render()
{
  ltm_render_alphanum(alphanum_string, block);
  ltm_render_numeric(numeric_string, block);
}

void parse_command(char *command)
{
  char *token;
  int found_cmd = 0;

  token = strtok(command, " \n");
  if (token == NULL) {
    token = command;
  }

  if (strcmp(token, "ALPHA") == 0) {
    found_cmd = 1;
  } else if (strcmp(token, "NUM") == 0) {
    found_cmd = 2;
  }

  token = strtok(NULL, "\n");

  if (token != NULL) {
    if (found_cmd == 1) {
      strncpy(alphanum_string, token, 8);
    } else if (found_cmd == 2) {
      strncpy(numeric_string, token, 5);
    }
  } else {
    if (found_cmd == 1) {
      alphanum_string[0] = '\0';
    } else if (found_cmd == 2) {
      numeric_string[0] = '\0';
    }
  }

  render();
}

int main(int argc, char *argv)
{
  int retval;
  int cmd_fd;
  int current_block = 0;
  int current_poll_timeout;
  struct pollfd cmd_poll[1];
  char command_buf[16];
  ssize_t bytes_read;

  /* Open the command pipe. */

  retval = mkfifo(CMD_PATH, 0640);
  if ((retval != 0) && (errno != EEXIST)) {
    perror("ltmy2kd: could not initialize command pipe");
    exit(1);
  }

  cmd_fd = open(CMD_PATH, O_RDONLY | O_NONBLOCK);

  cmd_poll[0].fd = cmd_fd;
  cmd_poll[0].events = POLLIN;

  /* Initialize the display. */

  retval = gpio_init();
  if (retval != 0) {
    fputs("ltmy2kd: error initializing GPIO\n", stderr);
    exit(1);
  }

  retval = ltm_display_init(GPIO_SEG_DATA, GPIO_SEG_CLOCK, GPIO_SEG_RESET);
  if (retval != 0) {
    fputs("ltmy2kd: error initializing display\n", stderr);
    exit(1);
  }

  ltm_clear();

  /* Enter the main loop.  We alternate between checking the pipe for
     new commands and refreshing/updating the display. */

  while (1) {

    /* Blast the current block to the display, but only if we have
       something to display.   If we're active, set up for the next
       block to display. */

    if ((alphanum_string[0] != '\0') || (numeric_string[0] != '\0')) {
      ltm_blast_block(block[current_block]);

      current_block++;
      if (current_block >= 5) {
	current_block = 0;
      }

      current_poll_timeout = POLL_TIMEOUT_DATA;
    } else {
      current_poll_timeout = POLL_TIMEOUT_BLANK;
    }

    /* Wait for a bit, watching for any incoming commands. */

    retval = poll(cmd_poll, 1, current_poll_timeout);

    /* Something weird happened during the poll. */

    if (retval < 0) {
      perror("ltmy2kd: error watching for command");
      exit(1);
    }

    /* Command data received. */
    else if (retval > 0) {
      bytes_read = read(cmd_fd, command_buf, 15);
      command_buf[bytes_read] = '\0';
      if (bytes_read > 0) {
	parse_command(command_buf);
      }
    }
  }
}
