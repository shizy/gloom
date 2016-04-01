# gloom

gloom is an X11 backlight dimming and cursor management tool for laptops. Aimed at saving battery life and getting rid of the annoying cursor when you don't need it.

## Installation

Install dependences through pacman:

```bash
$ pacman -S libxrandr libxfixes libxi xproto libx11
```

Then clone, build, and install:

```bash
$ git clone https://github.com/shizy/gloom.git
$ cd gloom
$ make
$ sudo make install
```

## Parameters

The addition of -c will cause the cursor to hide. The addition of either -s or -l will cause the backlight to dim.

```bash
$ gloom -c[n]           # hide the mouse cursor after [n] seconds of idle time (default 3)
$ gloom -s[n]           # dim the screen backlight after [n] seconds of idle time (default 45)
$ gloom -f[percent]     # fade to percentage of the current brightness level (default 50)
$ gloom -a              # dim the backlight regardless of whether a battery is found or not
$ gloom -h              # show usage help
```

## Signals

Sending a SIGUSR1 to gloom will prevent cursor hiding and screen dimming until another SIGUSR1 is sent:

```bash
$ kill -s SIGUSR1 $(pgrep gloom)
```

Useful to prevent the screen from dimming while watching a movie, etc.

## Examples

To just hide the cursor after 10 seconds of idle time:
```bash
$ gloom -c10
```

To hide the cursor after 10 seconds, and dim the screen after the default 45 seconds:
```bash
$ gloom -c10 -s
```

To dim the screen on a desktop PC after 1 minute to %15 of current brightness:
```bash
$ gloom -s60 -f15 -a
```
