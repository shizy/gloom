/*
 * What had happened to him? Why was his brow clammy with drops of moisture,
 * his knees shaking beneath him, and his soul oppressed with a cold gloom?
 * Was it because he had just seen these dreadful eyes again? Why, he had
 * left the Summer Garden on purpose to see them; that had been his "idea."
 * He had wished to assure himself that he would see them once more at that
 * house. Then why was he so overwhelmed now, having seen them as he expected?
 *
 * "The Idiot" (pt. II, ch. V) - Fyodor Dostoevsky
 */

#include <X11/extensions/Xrandr.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include <wait.h>

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

    pid_t lpid;

    unsigned int cursor_idle = 3,
                 screen_idle = 45,
                 screen_fade = 50,
                 lock_idle   = 600,
                 battery_int = 5,
                 bidle       = 0,
                 kidle       = 0,
                 lidle       = 0,
                 midle       = 0;

    bool cursor_conf  = false,
         screen_conf  = false,
         lock_conf    = false,
         battery_conf = true,
         battery      = false,
         dim          = false,
         cursor       = true, // check first?
         paused       = false,
         *locked;

    char locker[256] = "";

    void pause() { paused = !paused; }
    signal(SIGUSR1, pause);

    long brightness;

    for (int i = 1; i < argc; i++) {
        // set cursor timeout
        if (!strcmp(argv[i], "-c")) {
            cursor_conf = true;
            cursor_idle = (argc <= i + 1 || !isdigit((unsigned char) *argv[i + 1])) ? cursor_idle : strtol(argv[++i], NULL, 10);
        }
        // set screen timeout
        if (!strcmp(argv[i], "-s")) {
            screen_conf = true;
            screen_idle = (argc <= i + 1 || !isdigit((unsigned char) *argv[i + 1])) ? screen_idle : strtol(argv[++i], NULL, 10);
        }
        // set locker timeout
        if (!strcmp(argv[i], "-l")) {
            lock_conf = true;
            lock_idle = (argc <= i + 1 || !isdigit((unsigned char) *argv[i + 1])) ? lock_idle : strtol(argv[++i], NULL, 10);
        }

        // set brightness
        if (!strcmp(argv[i], "-f")) {
            screen_conf = true;
            screen_fade = (argc <= i + 1 || !isdigit((unsigned char) *argv[i + 1])) ? screen_fade : strtol(argv[++i], NULL, 10);
        }
        // set locker
        if (!strcmp(argv[i], "-k")) {
            lock_conf = true;
            if (argc <= i + 1) {
                printf("Error: -k requires an argument\n");
                exit(0);
            }
            strcpy(locker, argv[++i]);
        }
        // set always dim
        if (!strcmp(argv[i], "-a")) {
            battery_conf = false;
            battery = true;
        }

        // show help
        if (!strcmp(argv[i], "-h")) {
            printf("Usage: gloom [-c <cursor timeout>] [-s <screen timeout>] [-l <lock timeout>] [-f <brightness percent>] [-k <command>] [-a] [-h]");
            exit(0);
        }
    }
    if (lock_conf && !strcmp(locker, "")) {
        printf("Error: -l requires a locker to be set with -k\n");
        exit(0);
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

    // does a battery exist?
    if (battery_conf) {
        if (fopen("/sys/class/power_supply/BAT0/status", "r") == NULL) {
            battery_conf = false;
        } else {
            battery = battery_status();
        }
    }

    // shared variable for forked lock process
    if (lock_conf) {
        locked = mmap(NULL, sizeof *locked, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
        *locked = false;
    }

    for (;;) {

        (void) sleep(1);

        if (!paused) {

            kidle++;
            midle++;
            if (lock_conf) { lidle++; }

            // check battery status
            if (battery_conf) {

                bidle++;

                if (bidle == battery_int) {
                    bool pre_battery = battery;
                    battery = battery_status();
                    // if going to battery, reset idle time to catch screen_idle
                    if (!pre_battery && battery) {
                        kidle = 0;
                        midle = 0;
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
            if (cursor_conf && cursor && midle == cursor_idle) {
                XFixesHideCursor(dpy, w);
                XFlush(dpy);
                cursor = false;
            }

            // dim screen
            if (screen_conf && battery && !dim && kidle == screen_idle) {
                brightness = get_brightness(dpy, backlight, resources->outputs[0]);
                set_brightness(dpy, backlight, resources->outputs[0], (brightness * (screen_fade / 100)));
                dim = true;
            }

            // lock screen
            if (lock_conf && !*locked && lidle == lock_idle) {
                lpid = fork();
                if (lpid == 0) {
                    /* Child process */
                    *locked = true;
                    system(locker);
                    *locked = false;
                    _exit(0);
                    /* End child process */
                }
            }

            // activity
            while (XPending(dpy) > 0) {

                // interrupt, wait for activity
                XNextEvent(dpy, &e);
                kidle = 0;
                lidle = 0;

                // show cursor
                if (cursor_conf && !cursor && (e.xcookie.evtype == XI_RawMotion ||
                    e.xcookie.evtype == XI_RawButtonPress)) {
                        XFixesShowCursor(dpy, w);
                        XFlush(dpy);
                        cursor = true;
                        midle = 0;
                }

                // restore screen brightness
                if (screen_conf && dim) {
                    set_brightness(dpy, backlight, resources->outputs[0], brightness);
                    dim = false;
                }
            }
        }
    }

    munmap(locked, sizeof *locked);
    XRRFreeScreenResources(resources);
    return 0;
}
