// gcc -o gloom gloom.c -lX11 -lXrandr -lXfixes -lXi

#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

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

int main (int argc, char *argv[]) {

    int current, arg_index = 0;
    unsigned int cursor_conf = 0,
                 screen_conf = 0,
                 cursor_idle = 3,
                 screen_idle = 30,
                 screen_fade = 40;

    static struct option long_options[] = {
        { "cursor", optional_argument, 0, 'c' },
        { "screen", optional_argument, 0, 's' },
        { "level", optional_argument, 0, 'l' },
        { "help", no_argument, 0, 'h' },
        { 0, 0, 0, 0}
    };

    if (argc == 1) {
        printf("Too few arguments! -h for help\n");
        return (1);
    } else {
        while ((current = getopt_long(argc, argv, "c::s::l::h", long_options, &arg_index)) != -1) {

            switch (current) {
                // set cursor timeout
                case 'c':
                    cursor_conf = 1;
                    cursor_idle = (optarg == NULL) ? cursor_idle : strtol(optarg, NULL, 10);
                    break;
                // set screen timeout
                case 's':
                    screen_conf = 1;
                    screen_idle = (optarg == NULL) ? screen_idle : strtol(optarg, NULL, 10);
                    break;
                // set screen brightness
                case 'l':
                    screen_conf = 1;
                    screen_fade = (optarg == NULL) ? screen_fade : strtol(optarg, NULL, 10);
                    break;
                // show help
                case 'h':
                default:
                    printf("Usage: gloom [-c|--cursor <secs>] [-s|--screen <secs>] [-l|--level <brightness percent> ] [-h|--help]");
                    break;
            }
        }
    }

    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) {
        printf("Cannot connect to the X Server\n");
        return (1);
    }

    long brightness;
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

    unsigned int max_idle = (screen_idle > cursor_idle && screen_conf == 1) ? screen_idle : cursor_idle;
    unsigned int idle = 1;
    unsigned int dim = 0;
    unsigned int cursor = 1;

    for (;;) {

        // hide cursor
        if (cursor_conf == 1 && idle == cursor_idle) {
            XFixesHideCursor(dpy, w);
            cursor = 0;
        }

        // dim screen
        if (screen_conf == 1 && idle == screen_idle) {
            brightness = get_brightness(dpy, backlight, resources->outputs[0]);
            set_brightness(dpy, backlight, resources->outputs[0], (brightness * (screen_fade / 100)));
            dim = 1;
        }

        // activity (pre & post idle)
        while (XPending(dpy) > 0 || idle >= max_idle) {

            // interrupt, wait for activity
            XNextEvent(dpy, &e);
            idle = 0;

            // show cursor
            if (cursor_conf == 1 && cursor == 0 && (e.xcookie.evtype == XI_RawMotion ||
                e.xcookie.evtype == XI_RawButtonPress)) {
                    XFixesShowCursor(dpy, w);
                    cursor = 1;
            }

            // restore screen brightness
            if (screen_conf == 1 && dim == 1) {
                set_brightness(dpy, backlight, resources->outputs[0], brightness);
                dim = 0;

            }
        }

        (void) sleep(1);
        idle++;
    }

    XRRFreeScreenResources(resources);

    return 0;
}
