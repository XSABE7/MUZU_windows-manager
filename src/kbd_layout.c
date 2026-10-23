#define _GNU_SOURCE
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>
#include "kbd_layout.h"

static const char *english_codes[] = { "us", "gb", "au", "nz", "ie", NULL };

void kbd_layout_short_name(const char *code, char *out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';

    char base[32] = {0};
    size_t n = 0;
    if (code) {
        while (*code == ' ' || *code == '\t') code++;
        while (code[n] && code[n] != '(' && code[n] != ':' && code[n] != ' ' &&
               n + 1 < sizeof(base)) {
            base[n] = (char)tolower((unsigned char)code[n]);
            n++;
        }
    }
    base[n] = '\0';

    if (n == 0) {
        snprintf(out, out_len, "??");
        return;
    }
    for (int i = 0; english_codes[i]; i++) {
        if (strcmp(base, english_codes[i]) == 0) {
            snprintf(out, out_len, "EN");
            return;
        }
    }
    size_t i = 0;
    for (; i < 3 && base[i] && i + 1 < out_len; i++) {
        out[i] = (char)toupper((unsigned char)base[i]);
    }
    out[i] = '\0';
}

void kbd_layout_parse(KbdLayouts *kl, const char *layout_csv) {
    if (!kl) return;
    int saved_base = kl->xkb_event_base;
    memset(kl->codes, 0, sizeof(kl->codes));
    memset(kl->short_names, 0, sizeof(kl->short_names));
    kl->count = 0;
    kl->current_group = 0;
    kl->xkb_event_base = saved_base;
    if (!layout_csv) return;

    const char *p = layout_csv;
    while (*p && kl->count < KBD_MAX_GROUPS) {
        const char *end = strchr(p, ',');
        size_t len = end ? (size_t)(end - p) : strlen(p);

        /* trim whitespace */
        const char *s = p;
        while (len > 0 && (*s == ' ' || *s == '\t')) { s++; len--; }
        while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t')) len--;

        if (len > 0) {
            if (len >= sizeof(kl->codes[0])) len = sizeof(kl->codes[0]) - 1;
            memcpy(kl->codes[kl->count], s, len);
            kl->codes[kl->count][len] = '\0';
            kbd_layout_short_name(kl->codes[kl->count], kl->short_names[kl->count],
                                  sizeof(kl->short_names[0]));
            kl->count++;
        }
        if (!end) break;
        p = end + 1;
    }
}

int kbd_layout_is_unique(const KbdLayouts *kl, int group) {
    if (!kl || group < 0 || group >= kl->count) return 0;
    for (int i = 0; i < group; i++) {
        if (strcmp(kl->codes[i], kl->codes[group]) == 0) return 0;
    }
    return 1;
}

static int canonical_group(const KbdLayouts *kl, int group) {
    if (group < 0 || group >= kl->count) return 0;
    for (int i = 0; i < group; i++) {
        if (strcmp(kl->codes[i], kl->codes[group]) == 0) return i;
    }
    return group;
}

int kbd_layout_next_group(const KbdLayouts *kl, int current) {
    if (!kl || kl->count <= 0) return 0;
    int base = canonical_group(kl, current);
    for (int step = 1; step < kl->count; step++) {
        int g = (base + step) % kl->count;
        if (kbd_layout_is_unique(kl, g)) return g;
    }
    return base;
}

const char *kbd_layout_current_short(const KbdLayouts *kl) {
    if (!kl || kl->count <= 0) return "";
    int g = kl->current_group;
    if (g < 0 || g >= kl->count) g = 0;
    return kl->short_names[g];
}

static const struct { const char *code; const char *name; } layout_names[] = {
    { "us", "English (US)" }, { "gb", "English (UK)" }, { "th", "Thai" },
    { "jp", "Japanese" }, { "kr", "Korean" }, { "cn", "Chinese" },
    { "de", "German" }, { "fr", "French" }, { "es", "Spanish" },
    { "it", "Italian" }, { "ru", "Russian" }, { "la", "Lao" },
    { "vn", "Vietnamese" }, { "ara", "Arabic" }, { NULL, NULL }
};

const char *kbd_layout_display_name(const char *code) {
    if (!code) return "";
    size_t n = strcspn(code, "(:");
    for (int i = 0; layout_names[i].code; i++) {
        if (strlen(layout_names[i].code) == n && strncmp(code, layout_names[i].code, n) == 0) {
            return layout_names[i].name;
        }
    }
    return code;
}

/* ---------- X11 / XKB glue ---------- */

static void read_rules_layouts(Display *dpy, KbdLayouts *kl) {
    Atom rules_atom = XInternAtom(dpy, "_XKB_RULES_NAMES", True);
    if (rules_atom == None) {
        kbd_layout_parse(kl, "us");
        return;
    }
    Atom actual_type;
    int actual_format;
    unsigned long nitems = 0, bytes_after = 0;
    unsigned char *prop = NULL;
    int ok = XGetWindowProperty(dpy, DefaultRootWindow(dpy), rules_atom, 0, 1024, False,
                                XA_STRING, &actual_type, &actual_format,
                                &nitems, &bytes_after, &prop) == Success;
    if (ok && prop && nitems > 0 && actual_format == 8) {
        /* Layout: rules\0model\0layout\0variant\0options\0 */
        const char *s = (const char *)prop;
        unsigned long pos = 0;
        int field = 0;
        while (pos < nitems && field < 2) {
            if (s[pos] == '\0') field++;
            pos++;
        }
        char layouts[256] = {0};
        unsigned long j = 0;
        while (pos < nitems && s[pos] != '\0' && j + 1 < sizeof(layouts)) {
            layouts[j++] = s[pos++];
        }
        layouts[j] = '\0';
        kbd_layout_parse(kl, layouts[0] ? layouts : "us");
    } else {
        kbd_layout_parse(kl, "us");
    }
    if (prop) XFree(prop);
}

void kbd_layout_reload(Display *dpy, KbdLayouts *kl) {
    if (!dpy || !kl) return;
    read_rules_layouts(dpy, kl);
    if (kl->xkb_event_base >= 0) {
        XkbStateRec state;
        if (XkbGetState(dpy, XkbUseCoreKbd, &state) == Success) {
            kl->current_group = state.group;
            if (kl->current_group >= kl->count) kl->current_group = 0;
        }
    }
}

int kbd_layout_init(Display *dpy, KbdLayouts *kl) {
    if (!kl) return 0;
    memset(kl, 0, sizeof(*kl));
    kl->xkb_event_base = -1;
    if (!dpy) return 0;

    int opcode = 0, event_base = 0, error_base = 0;
    int major = XkbMajorVersion, minor = XkbMinorVersion;
    if (XkbQueryExtension(dpy, &opcode, &event_base, &error_base, &major, &minor)) {
        kl->xkb_event_base = event_base;
        XkbSelectEventDetails(dpy, XkbUseCoreKbd, XkbStateNotify,
                              XkbGroupStateMask, XkbGroupStateMask);
    }
    kbd_layout_reload(dpy, kl);
    return kl->xkb_event_base >= 0;
}

int kbd_layout_apply_layouts(const char *layouts) {
    if (!layouts || !layouts[0]) return 0;
    for (const unsigned char *p = (const unsigned char *)layouts; *p; p++) {
        if (!(isalnum(*p) || *p == ',' || *p == '_' || *p == '-' ||
              *p == '(' || *p == ')')) return 0;
    }
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        execlp("setxkbmap", "setxkbmap", "-layout", layouts, (char *)NULL);
        _exit(127);
    }
    int status = 0;
    return waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

void kbd_layout_set_group(Display *dpy, KbdLayouts *kl, int group) {
    if (!kl || group < 0 || group >= kl->count) return;
    kl->current_group = group;
    if (dpy && kl->xkb_event_base >= 0) {
        XkbLockGroup(dpy, XkbUseCoreKbd, (unsigned int)group);
        XFlush(dpy);
    }
}

void kbd_layout_cycle(Display *dpy, KbdLayouts *kl) {
    if (!kl || kl->count <= 1) return;
    kbd_layout_set_group(dpy, kl, kbd_layout_next_group(kl, kl->current_group));
}

int kbd_layout_handle_event(Display *dpy, KbdLayouts *kl, const XEvent *ev) {
    if (!dpy || !kl || !ev) return 0;
    if (kl->xkb_event_base >= 0 && ev->type == kl->xkb_event_base) {
        const XkbEvent *xe = (const XkbEvent *)ev;
        if (xe->any.xkb_type == XkbStateNotify) {
            int g = xe->state.group;
            if (g < 0 || g >= kl->count) g = 0;
            if (g != kl->current_group) {
                kl->current_group = g;
                return 1;
            }
        }
        return 0;
    }
    if (ev->type == PropertyNotify && ev->xproperty.window == DefaultRootWindow(dpy)) {
        Atom rules_atom = XInternAtom(dpy, "_XKB_RULES_NAMES", True);
        if (rules_atom != None && ev->xproperty.atom == rules_atom) {
            kbd_layout_reload(dpy, kl);
            return 1;
        }
    }
    return 0;
}
