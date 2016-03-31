// gcc -o gloom gloom.c -lX11 -lXrandr -lXfixes -lXi
// TODO
// track mouse and screen seperately
// add stop and start traps

#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdbool.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

void
check_extensions (Display *dpy) {
    int maj = 2,
        min = 2,
        opc = -1,
        ev, err;

    if (!XQueryExtension(dpy, "XInputExtension", &opc, &ev, &err)) {
        printf("Extension XInput not found.");
        exit(0);
    }

    if (XIQueryVersion(dpy, &maj, &min) != Success) {
        printf("Extension XI2 not found, or unsupported version");
        exit(0);
    }
}

static long
get_brightness (Display *dpy, Atom backlight, RROutput out) {

    unsigned long after, items;
    unsigned char *property;
    long brightness;
    int format;
    Atom type;

    if (XRRGetOutputProperty(dpy, out, backlight, 0, 4, False, False, None, &type, &format, &items, &after, &property) != Success) {
        return -1;
    }

    brightness = *((long *) property);
    XFree(property);
    return brightness;
}

static void
set_brightness (Display *dpy, Atom backlight, RROutput out, long brightness) {

    XRRChangeOutputProperty(dpy, out, backlight, XA_INTEGER, 32, PropModeReplace, (unsigned char *) &brightness, 1);
    XFlush(dpy);
}

static bool
battery_status () {
    FILE *f;
    char stat[12];
    if ((f = fopen("/sys/class/power_supply/BAT0/status", "r")) == NULL) {
        return false;
    }
    fgets(stat, 12, f);
    fclose(f);
    return (strcmp(stat, "Discharging") == 0) ? true : false;
}

int
main (int argc, char *argv[]) {

    int current, arg_index = 0;
    unsigned int cursor_idle = 3,
                 screen_idle = 30,
                 screen_fade = 40,
                 battery_int = 5,
                 bidle = 0,
                 idle = 0;

    bool cursor_conf = false,
         screen_conf = false,
         battery_conf = true,
         battery = false,
         dim = false,
         cursor = true;

    long brightness;

    static struct option long_options[] = {
        { "cursor", optional_argument, 0, 'c' },
        { "screen", optional_argument, 0, 's' },
        { "level", optional_argument, 0, 'l' },
        { "always", no_argument, 0, 'a' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0}
    };

    if (argc == 1) {
        printf("Too few arguments! -h for help\n");
        return (1);
    } else {
        while ((current = getopt_long(argc, argv, "c::s::l::ah", long_options, &arg_index)) != -1) {

            switch (current) {
                // set cursor timeout
                case 'c':
                    cursor_conf = true;
                    cursor_idle = (optarg == NULL) ? cursor_idle : strtol(optarg, NULL, 10);
                    break;
                // set screen timeout
                case 's':
                    screen_conf = true;
                    screen_idle = (optarg == NULL) ? screen_idle : strtol(optarg, NULL, 10);
                    break;
                // set screen brightness
                case 'l':
                    screen_conf = true;
                    screen_fade = (optarg == NULL) ? screen_fade : strtol(optarg, NULL, 10);
                    break;
                // always dim
                case 'a':
                    battery_conf = false;
                    battery = true;
                    break;
                // show help
                case 'h':
                default:
                    printf("Usage: gloom [-c|--cursor <secs>] [-s|--screen <secs>] [-l|--level <brightness percent> ] [-a|--always] [-h|--help]");
                    break;
            }
        }
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        printf("Cannot connect to the X Server\n");
        exit(0);
    }
    check_extensions(dpy);

    Window w = DefaultRootWindow(dpy);
    Atom backlight = XInternAtom(dpy, "Backlight", True);
    XRRScreenResources *resources = XRRGetScreenResources(dpy, w);
    XIEventMask masks[1];
    unsigned char mask[(XI_LASTEVENT + 7)/8];
    memset(mask, 0, sizeof(mask));
    XISetMask(mask, XI_RawMotion);
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawKeyPress);
    masks[0].deviceid = XIAllMasterDevices;
    masks[0].mask_len = sizeof(mask);
    masks[0].mask = mask;
    XISelectEvents(dpy, w, masks, 1);
    XFlush(dpy);
    XEvent e;

    if (battery_conf) {
        // does a battery exist?
        if (fopen("/sys/class/power_supply/BAT0/status", "r") == NULL) {
            battery_conf = false;
        } else {
            battery = battery_status();
        }
    }

    for (;;) {

        (void) sleep(1);
        idle++;

        // check battery status
        if (battery_conf) {

            bidle++;

            if (bidle == battery_int) {
                bool pre_battery = battery;
                battery = battery_status();
                // if going to battery, reset idle time to catch screen_idle
                if (!pre_battery && battery) {
                    idle = 0;
                }
                // if going to AC, re-brighten the screen if dim
                if (pre_battery && !battery && screen_conf && dim) {
                    set_brightness(dpy, backlight, resources->outputs[0], brightness);
                    dim = false;
                }
                bidle = 0;
            }
        }

        // hide cursor
        if (cursor_conf && cursor && idle == cursor_idle) {
            XFixesHideCursor(dpy, w);
            XFlush(dpy);
            cursor = false;
        }

        // dim screen
        if (screen_conf && battery && !dim && idle == screen_idle) {
            brightness = get_brightness(dpy, backlight, resources->outputs[0]);
            set_brightness(dpy, backlight, resources->outputs[0], (brightness * (screen_fade / 100)));
            dim = true;
        }

        // activity
        while (XPending(dpy) > 0) {

            // interrupt, wait for activity
            XNextEvent(dpy, &e);
            idle = 0;

            // show cursor
            if (cursor_conf && !cursor && (e.xcookie.evtype == XI_RawMotion ||
                e.xcookie.evtype == XI_RawButtonPress)) {
                    XFixesShowCursor(dpy, w);
                    XFlush(dpy);
                    cursor = true;
            }

            // restore screen brightness
            if (screen_conf && dim) {
                set_brightness(dpy, backlight, resources->outputs[0], brightness);
                dim = false;
            }
        }
    }

    XRRFreeScreenResources(resources);
    return 0;
}
