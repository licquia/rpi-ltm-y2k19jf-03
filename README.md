# ltmy2kd -- display server for the LTMY2K19JF-03 module

This is a display server to drive a LTMY2K19JF-03 display module on
Linux.  It currently assumes a Raspberry Pi (and with a specific pin
configuration), but should be easily ported to any Linux with support
for Pi-style GPIO pins.

What a LTMY2K-whatsis, you may ask?  A while back, Lite-On made this
LED display module that used 7-segment and 14-segment displays, along
with a few icons, probably as a specialized display for some desktop
system or case.  It was reverse engineered by David Cook of Robot
Room; more information here:

http://www.robotroom.com/MultiSegmentLEDDisplay.html

I was given one of these by a friend, and thought it would be fun and
useful to write a display server for my Raspberry Pi.  Since the
display is driven by a STTI ST2225A LED driver, this code might also
be useful for other projects using that chip.

## Prerequisites

To build this, you'll need a C compiler and make.

You probably also want the wiringPi library for controlling GPIO pins:

http://wiringpi.com/

We also include support for using standard sysfs GPIO support, but
it's very slow and flickery (more about this later).  It shouldn't be
too hard to adapt this to other GPIO control libraries.

## Installation

It's old-school autoconf:

```
# ./configure
# make
# make install
```

At this point, you've got the ltmy2kd binary in /usr/local/sbin and a
System V init script in /etc/init.d/ltmy2kd.  You should be able to
start it using your init system's startup tools.  Something like
`service ltmy2kd start` should do the trick.

You can also `make uninstall`, which removes the binary and init
script.  Note that, if you've set up your init system to start the
service on boot, it's up to you to do the right thing to uninstall
those bits.

## Use

The service creates a communication FIFO in /run/ltmy2kd, which you
can use to send commands.  Two commands are currently supported:

* ALPHA - write the alphanumeric string to the 14-segment LED
  characters at the top of the display.  Spaces are allowed (but they
  count as a character, of course).  Unrecognized characters get
  turned into an asterisk.  This includes lowercase characters; you
  definitely want to stick to uppercase.

* NUM - write the numeric string to the 7-segment LED digits at the
  bottom of the display.  Unrecognized characters get turned into a
  dash.

To clear out what's currently displayed, send just a command with a
blank string.

Here's an example script that displays a timed message:

```
#!/bin/sh

echo "ALPHA TESTING" > /run/ltmy2kd
sleep 5
echo "ALPHA ONE" > /run/ltmy2kd
sleep 5
echo "ALPHA 2 AND" > /run/ltmy2kd
sleep 5
echo "ALPHA THREE" > /run/ltmy2kd
sleep 5
echo "ALPHA" > /run/ltmy2kd
```

The display also supports two colons (with each dot individually
addressable) and four icons: mail, phone, cylinder, and network.
These aren't currently supported by this service, though it wouldn't
be too hard to add support.  Patches welcome, or if there's interest,
I could be persuaded to add it.

## CPU Usage

You'll probably notice that the service uses practically no CPU until
it's time to display something, at which point it seems to use quite a
bit of CPU for what seems like a simple thing (10-15% on my Pi).
What's it doing?

It turns out that there are about 140 separately addressable LEDs in
this display, but the driver chip only supports about 40 LEDs at a
time.  So, the display was built with a switching facility, where only
part of the display can be turned on at a time.  To use the whole
display, the service has to keep track of 5 groups of LEDs and switch
rapidly between those groups--quickly enough so that they appear
steadily lit.

To make things worse, timing on Linux kernels for delays under about
100 microseconds are best done with busy-wait loops, which consume CPU
while they're happening.  Those busy loops are perhaps the most
resource-intensive parts of this service.  It doesn't help that we
need high scheduler priority as well.

If the CPU usage is too high for you, you're welcome to play with the
timings.  As things slow down, though, you'll begin to see flicker in
the display.

The flicker is particularly bad if you're using the sysfs GPIO
interface to update the display instead of wiringPi, because the
overhead of writing to sysfs is so high.

Jeff Licquia
jeff@licquia.org
