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
#include <sched.h>
#include <syslog.h>
#include <signal.h>

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

/* PID file, to prevent running more than once. */

#define PID_FILE "/run/ltmy2kd.pid"

/* Timeouts for polling.  We set a long timeout when the display is
   blank, because we don't have to really do anything in that case. */

#define POLL_TIMEOUT_BLANK 5000
#define POLL_TIMEOUT_DATA 2

/* Global state. */

char alphanum_string[8] = "";
char numeric_string[5] = "";

uint8_t block[5][5] = 
  { { 0x00, 0x00, 0x00, 0x04, 0x00 },
    { 0x00, 0x00, 0x00, 0x02, 0x00 },
    { 0x00, 0x00, 0x00, 0x01, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x80 },
    { 0x00, 0x00, 0x00, 0x00, 0x40 } };

/* Error reporting after daemonizing. */

void record_errno_error(const char *errmsg)
{
  syslog(LOG_ERR, "%s: %s", errmsg, strerror(errno));
}

/* Parse a command string and render its result. */

void parse_command(char *command)
{
  char *token;
  int found_cmd = 0;

  /* Figure out which command was given. */

  token = strtok(command, " \n");
  if (token == NULL) {
    token = command;
  }

  if (strcmp(token, "ALPHA") == 0) {
    found_cmd = 1;
  } else if (strcmp(token, "NUM") == 0) {
    found_cmd = 2;
  }

  /* Read the rest of the line as the string to output. */

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

  /* Render the current results. */

  ltm_render_alphanum(alphanum_string, block);
  ltm_render_numeric(numeric_string, block);
}

int main(int argc, char *argv)
{
  pid_t pid;
  int retval;
  int pid_file_fd, cmd_fd, cmd_write_fd;
  int current_block = 0;
  int current_poll_timeout;
  struct pollfd cmd_poll[1];
  char command_buf[16];
  char pid_buf[8];
  ssize_t bytes_read;
  struct sched_param policy_param;

  /* Daemonize. */

  pid = fork();
  if (pid < 0) {
    perror("ltmy2kd: could not fork");
    exit(1);
  } else if (pid > 0) {
    exit(0);
  }

  umask(0);
  setsid();
  chdir("/");
  close(0);
  close(1);
  close(2);

  syslog(LOG_INFO, "starting, PID %d", getpid());

  /* Check for PID file, and write it. */

  pid_file_fd = open(PID_FILE, O_WRONLY | O_CREAT | O_EXCL);
  while (pid_file_fd < 0) {
    if (errno == EEXIST) {
      pid_file_fd = open(PID_FILE, O_RDONLY);
      if (pid_file_fd >= 0) {
        bytes_read = read(pid_file_fd, pid_buf, 7);
        if (bytes_read > 0) {
          pid_buf[bytes_read] = '\0';
          pid = strtol(pid_buf, NULL, 10);
          close(pid_file_fd);
          if (kill(pid, 0) == 0) {
            syslog(LOG_ERR, "another process found (%d)", pid);
            exit(1);
          }
          unlink(PID_FILE);
          pid_file_fd = open(PID_FILE, O_WRONLY | O_CREAT | O_EXCL);
        }
      } else {
        record_errno_error("error reading existing pid file");
        exit(1);
      }
    } else {
      record_errno_error("error writing pid file");
      exit(1);
    }
  }

  sprintf(pid_buf, "%d\n", getpid());
  write(pid_file_fd, pid_buf, strlen(pid_buf));
  close(pid_file_fd);

  /* Set up logging. */

  openlog("ltmy2kd", 0, LOG_DAEMON);

  /* Set process priority. */

  policy_param.sched_priority = 1;
  sched_setscheduler(0, SCHED_FIFO, &policy_param);

  /* Open the command pipe. */

  retval = mkfifo(CMD_PATH, 0640);
  if ((retval != 0) && (errno != EEXIST)) {
    record_errno_error("could not initialize command pipe");
    exit(1);
  }

  cmd_fd = open(CMD_PATH, O_RDONLY | O_NONBLOCK);

  cmd_write_fd = open(CMD_PATH, O_WRONLY);

  cmd_poll[0].fd = cmd_fd;
  cmd_poll[0].events = POLLIN;

  /* Initialize the display. */

  retval = gpio_init();
  if (retval != 0) {
    syslog(LOG_ERR, "error initializing GPIO");
    exit(1);
  }

  retval = ltm_display_init(GPIO_SEG_DATA, GPIO_SEG_CLOCK, GPIO_SEG_RESET);
  if (retval != 0) {
    syslog(LOG_ERR, "error initializing display");
    exit(1);
  }

  ltm_clear();

  /* Enter the main loop.  We alternate between checking the pipe for
     new commands and refreshing/updating the display. */

  while (1) {

    /* Blast the current block to the display. */

    ltm_blast_block(block[current_block]);

    /* Set up the next block to blast. */

    current_block++;
    if (current_block >= 5) {
      current_block = 0;
    }

    /* Set the delay until the next refresh.  As a special case, wait
       a really long time if our current strings are blank, so we take
       very little CPU when we don't need to do anything. */

    if ((alphanum_string[0] != '\0') || (numeric_string[0] != '\0')) {
      current_poll_timeout = POLL_TIMEOUT_DATA;
    } else {
      current_poll_timeout = POLL_TIMEOUT_BLANK;
    }

    /* Wait for a bit, watching for any incoming commands. */

    retval = poll(cmd_poll, 1, current_poll_timeout);

    /* Something weird happened during the poll. */

    if (retval < 0) {
      record_errno_error("error watching for command");
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
