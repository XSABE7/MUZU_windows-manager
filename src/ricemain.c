#define _GNU_SOURCE
#include <Python.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>

#include "ricemain.h"
#include "menu.h"
#include "config_rules.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static RiceRadialMenu g_radial_menu;
static int g_python_initialized = 0;
static Visual *g_visual = NULL;
static int g_depth = 0;
static Colormap g_colormap = None;

static void ricemain_radial_draw(void);

/* --- Python C Extension Methods --- */

static PyObject *py_muzuwm_add_radial_item(PyObject *self, PyObject *args, PyObject *keywds) {
    (void)self;
    const char *id = NULL;
    const char *title = NULL;
    const char *icon = "";
    const char *cmd = "";
    int action_id = 0;
    static char *kwlist[] = {"id", "title", "icon", "command", "action_id", NULL};

    if (!PyArg_ParseTupleAndKeywords(args, keywds, "ss|ssi", kwlist,
                                     &id, &title, &icon, &cmd, &action_id)) {
        return NULL;
    }

    int res = ricemain_radial_add_item(id, title, icon, cmd, action_id);
    return PyBool_FromLong(res);
}

static PyObject *py_muzuwm_clear_radial_items(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    ricemain_radial_clear_items();
    Py_RETURN_NONE;
}

static PyObject *py_muzuwm_spawn(PyObject *self, PyObject *args) {
    (void)self;
    const char *cmd = NULL;
    if (!PyArg_ParseTuple(args, "s", &cmd)) return NULL;
    config_spawn_command(cmd);
    Py_RETURN_NONE;
}

static PyObject *py_muzuwm_open_terminal(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    if (g_radial_menu.action_cb) {
        g_radial_menu.action_cb(DESKTOP_ACTION_TERMINAL, "", g_radial_menu.userdata);
    }
    Py_RETURN_NONE;
}

static PyObject *py_muzuwm_open_settings(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    if (g_radial_menu.action_cb) {
        g_radial_menu.action_cb(DESKTOP_ACTION_SETTINGS, "", g_radial_menu.userdata);
    }
    Py_RETURN_NONE;
}

static PyObject *py_muzuwm_open_apps(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    if (g_radial_menu.action_cb) {
        g_radial_menu.action_cb(DESKTOP_ACTION_APPLICATIONS, "", g_radial_menu.userdata);
    }
    Py_RETURN_NONE;
}

static PyObject *py_muzuwm_get_screen_size(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    return Py_BuildValue("(ii)", g_radial_menu.screen_w, g_radial_menu.screen_h);
}

static PyObject *py_muzuwm_is_radial_visible(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    return PyBool_FromLong(g_radial_menu.is_visible);
}

static PyObject *py_muzuwm_show_radial(PyObject *self, PyObject *args) {
    (void)self;
    int x = 0, y = 0;
    if (!PyArg_ParseTuple(args, "ii", &x, &y)) return NULL;
    if (g_radial_menu.dpy) {
        ricemain_radial_show(g_radial_menu.dpy, x, y);
    }
    Py_RETURN_NONE;
}

static PyObject *py_muzuwm_hide_radial(PyObject *self, PyObject *args) {
    (void)self;
    (void)args;
    if (g_radial_menu.dpy) {
        ricemain_radial_hide(g_radial_menu.dpy);
    }
    Py_RETURN_NONE;
}

static PyMethodDef MuzuwmMethods[] = {
    {"add_radial_item", (PyCFunction)(void(*)(void))py_muzuwm_add_radial_item, METH_VARARGS | METH_KEYWORDS, "Add an item to the radial menu"},
    {"clear_radial_items", py_muzuwm_clear_radial_items, METH_NOARGS, "Clear all radial menu items"},
    {"spawn", py_muzuwm_spawn, METH_VARARGS, "Spawn a command asynchronously"},
    {"open_terminal", py_muzuwm_open_terminal, METH_NOARGS, "Open default terminal"},
    {"open_settings", py_muzuwm_open_settings, METH_NOARGS, "Open MUZUwm settings"},
    {"open_apps", py_muzuwm_open_apps, METH_NOARGS, "Open application launcher"},
    {"get_screen_size", py_muzuwm_get_screen_size, METH_NOARGS, "Get (width, height) of screen"},
    {"is_radial_visible", py_muzuwm_is_radial_visible, METH_NOARGS, "Check if radial menu is visible"},
    {"show_radial", py_muzuwm_show_radial, METH_VARARGS, "Show radial menu at (x, y)"},
    {"hide_radial", py_muzuwm_hide_radial, METH_NOARGS, "Hide radial menu"},
    {NULL, NULL, 0, NULL}
};

static struct PyModuleDef muzuwm_module = {
    PyModuleDef_HEAD_INIT,
    "muzuwm",
    "MUZUwm Extension and Rice API",
    -1,
    MuzuwmMethods,
    NULL, NULL, NULL, NULL
};

PyMODINIT_FUNC PyInit_muzuwm(void) {
    PyObject *m = PyModule_Create(&muzuwm_module);
    if (!m) return NULL;

    PyModule_AddIntConstant(m, "ACTION_WALLPAPER_BROWSE", DESKTOP_ACTION_WALLPAPER_BROWSE);
    PyModule_AddIntConstant(m, "ACTION_WALLPAPER_PATH", DESKTOP_ACTION_WALLPAPER_PATH);
    PyModule_AddIntConstant(m, "ACTION_TERMINAL", DESKTOP_ACTION_TERMINAL);
    PyModule_AddIntConstant(m, "ACTION_APPLICATIONS", DESKTOP_ACTION_APPLICATIONS);
    PyModule_AddIntConstant(m, "ACTION_SETTINGS", DESKTOP_ACTION_SETTINGS);

    return m;
}

/* --- Extension Loader (.rim) --- */

static void load_rim_file(const char *path) {
    if (!path || access(path, R_OK) != 0) return;
    FILE *f = fopen(path, "r");
    if (!f) return;

    fprintf(stderr, "[ricewm] Loading extension: %s\n", path);
    PyRun_SimpleFile(f, path);
    fclose(f);
}

static void ricemain_load_extensions_from_dir(const char *dir_path) {
    if (!dir_path) return;
    DIR *d = opendir(dir_path);
    if (!d) return;

    struct dirent *ent;
    char fullpath[1024];
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(ent->d_name + len - 4, ".rim") == 0) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_path, ent->d_name);
            load_rim_file(fullpath);
        }
    }
    closedir(d);
}

static void ricemain_load_all_extensions(void) {
    const char *home = getenv("HOME");
    char user_ext_dir[512];
    if (home) {
        snprintf(user_ext_dir, sizeof(user_ext_dir), "%s/.config/muzuwm/extensions", home);
        mkdir(user_ext_dir, 0755);
        ricemain_load_extensions_from_dir(user_ext_dir);
    }

    ricemain_load_extensions_from_dir("./extensions");
    ricemain_load_extensions_from_dir("/usr/share/muzuwm/extensions");
}

/* --- Radial Menu Implementation --- */

static void ricemain_setup_default_items(void) {
    if (g_radial_menu.item_count > 0) return;
    ricemain_radial_add_item("settings", "Settings", "preferences-system", "", DESKTOP_ACTION_SETTINGS);
    ricemain_radial_add_item("apps", "App Menu", "system-run", "", DESKTOP_ACTION_APPLICATIONS);
    ricemain_radial_add_item("terminal", "Terminal", "utilities-terminal", "", DESKTOP_ACTION_TERMINAL);
}

int ricemain_radial_add_item(const char *id, const char *title, const char *icon, const char *cmd, int action_id) {
    if (!id || !title || g_radial_menu.item_count >= RICEMAIN_MAX_ITEMS) return 0;
    RiceRadialItem *item = &g_radial_menu.items[g_radial_menu.item_count++];
    snprintf(item->id, sizeof(item->id), "%s", id);
    snprintf(item->title, sizeof(item->title), "%s", title);
    snprintf(item->icon, sizeof(item->icon), "%s", icon ? icon : "");
    snprintf(item->command, sizeof(item->command), "%s", cmd ? cmd : "");
    item->action_id = action_id;
    return 1;
}

void ricemain_radial_clear_items(void) {
    g_radial_menu.item_count = 0;
    g_radial_menu.hover_idx = -1;
}

int ricemain_init(Display *dpy, Window root, int screen_w, int screen_h,
                  const GtkTheme *theme,
                  void (*action_cb)(int action_id, const char *cmd, void *userdata),
                  void *userdata) {
    memset(&g_radial_menu, 0, sizeof(RiceRadialMenu));
    g_radial_menu.dpy = dpy;
    g_radial_menu.root = root;
    g_radial_menu.screen_w = screen_w;
    g_radial_menu.screen_h = screen_h;
    g_radial_menu.theme = theme;
    g_radial_menu.action_cb = action_cb;
    g_radial_menu.userdata = userdata;
    g_radial_menu.hover_idx = -1;

    g_radial_menu.width = RICEMAIN_RADIUS * 2 + 30;
    g_radial_menu.height = RICEMAIN_RADIUS * 2 + 30;

    if (dpy) {
        int screen = DefaultScreen(dpy);
        XVisualInfo vinfo;
        if (XMatchVisualInfo(dpy, screen, 32, TrueColor, &vinfo)) {
            g_visual = vinfo.visual;
            g_depth = 32;
            g_colormap = XCreateColormap(dpy, root, g_visual, AllocNone);
        } else {
            g_visual = DefaultVisual(dpy, screen);
            g_depth = DefaultDepth(dpy, screen);
            g_colormap = None;
        }

        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.colormap = g_colormap != None ? g_colormap : DefaultColormap(dpy, screen);
        attrs.border_pixel = 0;
        attrs.background_pixel = 0;
        attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                           PointerMotionMask | KeyPressMask | StructureNotifyMask;

        unsigned long valuemask = CWOverrideRedirect | CWBorderPixel | CWBackPixel | CWEventMask;
        if (g_depth == 32 && g_colormap != None) {
            valuemask |= CWColormap;
        }

        g_radial_menu.win = XCreateWindow(dpy, root, 0, 0,
                                          g_radial_menu.width, g_radial_menu.height, 0,
                                          g_depth, InputOutput, g_visual,
                                          valuemask, &attrs);

        Atom net_wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        Atom net_wm_popup = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
        XChangeProperty(dpy, g_radial_menu.win, net_wm_type, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)&net_wm_popup, 1);

        g_radial_menu.surface = cairo_xlib_surface_create(dpy, g_radial_menu.win,
                                                          g_visual,
                                                          g_radial_menu.width,
                                                          g_radial_menu.height);
    }

    /* Initialize Python runtime */
    if (!g_python_initialized) {
        PyImport_AppendInittab("muzuwm", PyInit_muzuwm);
        Py_Initialize();
        g_python_initialized = 1;

        /* Execute python code to ensure sys.path includes config extensions */
        char py_init_code[1024];
        snprintf(py_init_code, sizeof(py_init_code),
                 "import sys, os\n"
                 "ext_dir = os.path.expanduser('~/.config/muzuwm/extensions')\n"
                 "if ext_dir not in sys.path: sys.path.insert(0, ext_dir)\n"
                 "if './extensions' not in sys.path: sys.path.insert(0, './extensions')\n");
        PyRun_SimpleString(py_init_code);
    }

    /* Load .rim extensions */
    ricemain_load_all_extensions();

    /* Setup defaults if no extensions populated items */
    ricemain_setup_default_items();

    return 1;
}

void ricemain_cleanup(void) {
    if (g_radial_menu.surface) {
        cairo_surface_finish(g_radial_menu.surface);
        cairo_surface_destroy(g_radial_menu.surface);
        g_radial_menu.surface = NULL;
    }
    if (g_radial_menu.dpy && g_radial_menu.win != None) {
        XDestroyWindow(g_radial_menu.dpy, g_radial_menu.win);
        g_radial_menu.win = None;
    }
    if (g_radial_menu.dpy && g_colormap != None) {
        XFreeColormap(g_radial_menu.dpy, g_colormap);
        g_colormap = None;
    }

    if (g_python_initialized) {
        Py_Finalize();
        g_python_initialized = 0;
    }
}

void ricemain_reload(Display *dpy) {
    ricemain_radial_clear_items();
    ricemain_load_all_extensions();
    ricemain_setup_default_items();
    if (g_radial_menu.is_visible) {
        ricemain_radial_draw();
        if (dpy) XFlush(dpy);
    }
}

int ricemain_is_radial_visible(void) {
    return g_radial_menu.is_visible;
}

void ricemain_radial_show(Display *dpy, int x, int y) {
    if (!dpy || g_radial_menu.win == None) return;

    g_radial_menu.center_x = x;
    g_radial_menu.center_y = y;

    int win_x = x - g_radial_menu.width / 2;
    int win_y = y - g_radial_menu.height / 2;

    /* Clamp inside screen boundary */
    if (win_x < 0) win_x = 0;
    if (win_y < 0) win_y = 0;
    if (win_x + g_radial_menu.width > g_radial_menu.screen_w) {
        win_x = g_radial_menu.screen_w - g_radial_menu.width;
    }
    if (win_y + g_radial_menu.height > g_radial_menu.screen_h) {
        win_y = g_radial_menu.screen_h - g_radial_menu.height;
    }

    XMoveWindow(dpy, g_radial_menu.win, win_x, win_y);
    XMapRaised(dpy, g_radial_menu.win);
    g_radial_menu.is_visible = 1;
    g_radial_menu.hover_idx = -1;

    ricemain_radial_draw();

    XGrabPointer(dpy, g_radial_menu.win, True,
                 ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                 GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    XGrabKeyboard(dpy, g_radial_menu.win, True,
                  GrabModeAsync, GrabModeAsync, CurrentTime);

    XFlush(dpy);
}

void ricemain_radial_hide(Display *dpy) {
    if (!g_radial_menu.is_visible) return;
    g_radial_menu.is_visible = 0;
    g_radial_menu.hover_idx = -1;

    if (dpy && g_radial_menu.win != None) {
        XUngrabPointer(dpy, CurrentTime);
        XUngrabKeyboard(dpy, CurrentTime);
        XUnmapWindow(dpy, g_radial_menu.win);
        XFlush(dpy);
    }
}

static void ricemain_radial_draw(void) {
    if (!g_radial_menu.surface) return;

    cairo_t *cr = cairo_create(g_radial_menu.surface);
    if (!cr) return;

    double w = g_radial_menu.width;
    double h = g_radial_menu.height;
    double cx = w / 2.0;
    double cy = h / 2.0;
    double r_out = RICEMAIN_RADIUS;
    double r_in = RICEMAIN_INNER_RADIUS;

    /* Clear surface (transparent if 32-bit ARGB, solid dark otherwise) */
    if (g_depth == 32) {
        cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
        cairo_paint(cr);
        cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
    } else {
        cairo_set_source_rgb(cr, 0.10, 0.10, 0.12);
        cairo_paint(cr);
    }

    int count = g_radial_menu.item_count;
    if (count <= 0) {
        cairo_destroy(cr);
        return;
    }

    double angle_step = (2.0 * M_PI) / (double)count;
    double start_angle_base = -M_PI / 2.0 - angle_step / 2.0;

    /* Draw slices */
    for (int i = 0; i < count; i++) {
        double a1 = start_angle_base + i * angle_step;
        double a2 = a1 + angle_step;

        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, r_out, a1, a2);
        cairo_arc_negative(cr, cx, cy, r_in, a2, a1);
        cairo_close_path(cr);

        if (i == g_radial_menu.hover_idx) {
            /* Active highlight */
            cairo_set_source_rgba(cr, 0.22, 0.52, 0.90, 0.95);
        } else {
            /* Sleek dark glass */
            cairo_set_source_rgba(cr, 0.14, 0.15, 0.20, 0.90);
        }
        cairo_fill_preserve(cr);

        /* Slice border */
        cairo_set_source_rgba(cr, 0.30, 0.34, 0.44, 0.65);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        /* Draw label text */
        double mid_a = (a1 + a2) / 2.0;
        double mid_r = (r_out + r_in) / 2.0;
        double tx = cx + mid_r * cos(mid_a);
        double ty = cy + mid_r * sin(mid_a);

        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 12.0);

        cairo_text_extents_t ext;
        cairo_text_extents(cr, g_radial_menu.items[i].title, &ext);
        cairo_move_to(cr, tx - ext.width / 2.0 - ext.x_bearing,
                          ty - ext.height / 2.0 - ext.y_bearing);

        if (i == g_radial_menu.hover_idx) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        } else {
            cairo_set_source_rgba(cr, 0.88, 0.90, 0.94, 0.95);
        }
        cairo_show_text(cr, g_radial_menu.items[i].title);
    }

    /* Outer glowing circle rim */
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, r_out + 1.0, 0, 2.0 * M_PI);
    cairo_set_source_rgba(cr, 0.40, 0.45, 0.58, 0.40);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    /* Center Hub */
    cairo_new_path(cr);
    cairo_arc(cr, cx, cy, r_in, 0, 2.0 * M_PI);
    cairo_set_source_rgba(cr, 0.10, 0.11, 0.15, 0.98);
    cairo_fill_preserve(cr);

    cairo_set_source_rgba(cr, 0.35, 0.40, 0.52, 0.85);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    /* Center indicator or active item name */
    if (g_radial_menu.hover_idx >= 0 && g_radial_menu.hover_idx < count) {
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10.0);
        cairo_set_source_rgba(cr, 0.40, 0.75, 1.0, 1.0);
        cairo_text_extents_t cext;
        cairo_text_extents(cr, g_radial_menu.items[g_radial_menu.hover_idx].title, &cext);
        cairo_move_to(cr, cx - cext.width / 2.0 - cext.x_bearing,
                          cy - cext.height / 2.0 - cext.y_bearing);
        cairo_show_text(cr, g_radial_menu.items[g_radial_menu.hover_idx].title);
    } else {
        cairo_new_path(cr);
        cairo_arc(cr, cx, cy, 6.0, 0, 2.0 * M_PI);
        cairo_set_source_rgba(cr, 0.30, 0.60, 0.95, 0.9);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
}

static int ricemain_calculate_hover(int mx, int my) {
    double cx = g_radial_menu.width / 2.0;
    double cy = g_radial_menu.height / 2.0;
    double dx = (double)mx - cx;
    double dy = (double)my - cy;
    double dist = hypot(dx, dy);

    if (dist < RICEMAIN_INNER_RADIUS || dist > RICEMAIN_RADIUS) {
        return -1;
    }

    int count = g_radial_menu.item_count;
    if (count <= 0) return -1;

    double angle_step = (2.0 * M_PI) / (double)count;
    double start_angle_base = -M_PI / 2.0 - angle_step / 2.0;
    double angle = atan2(dy, dx);

    double diff = angle - start_angle_base;
    while (diff < 0.0) diff += 2.0 * M_PI;
    while (diff >= 2.0 * M_PI) diff -= 2.0 * M_PI;

    int idx = (int)(diff / angle_step);
    if (idx < 0) idx = 0;
    if (idx >= count) idx = count - 1;
    return idx;
}

int ricemain_handle_root_click(Display *dpy, int button, int root_x, int root_y) {
    if (!config_is_ricewm_enabled()) return 0;

    char mode[32];
    config_get_desktop_menu_mode(mode, sizeof(mode));
    if (strcasecmp(mode, "radial") != 0) {
        return 0;
    }

    if (button == Button3) {
        ricemain_radial_show(dpy, root_x, root_y);
        return 1;
    }
    return 0;
}

int ricemain_handle_event(Display *dpy, XEvent *ev) {
    if (!g_radial_menu.is_visible) return 0;

    if (ev->type == Expose && ev->xexpose.window == g_radial_menu.win) {
        ricemain_radial_draw();
        return 1;
    }

    if (ev->type == MotionNotify) {
        int x = ev->xmotion.x;
        int y = ev->xmotion.y;
        if (ev->xmotion.window != g_radial_menu.win) {
            /* Convert root coordinates to local window */
            int win_x = 0, win_y = 0;
            Window child;
            XTranslateCoordinates(dpy, g_radial_menu.root, g_radial_menu.win,
                                  ev->xmotion.x_root, ev->xmotion.y_root,
                                  &win_x, &win_y, &child);
            x = win_x;
            y = win_y;
        }

        int new_hover = ricemain_calculate_hover(x, y);
        if (new_hover != g_radial_menu.hover_idx) {
            g_radial_menu.hover_idx = new_hover;
            ricemain_radial_draw();
            XFlush(dpy);
        }
        return 1;
    }

    if (ev->type == ButtonPress) {
        int x = ev->xbutton.x;
        int y = ev->xbutton.y;
        if (ev->xbutton.window != g_radial_menu.win) {
            int win_x = 0, win_y = 0;
            Window child;
            XTranslateCoordinates(dpy, g_radial_menu.root, g_radial_menu.win,
                                  ev->xbutton.x_root, ev->xbutton.y_root,
                                  &win_x, &win_y, &child);
            x = win_x;
            y = win_y;
        }

        int clicked_idx = ricemain_calculate_hover(x, y);
        if (clicked_idx >= 0 && clicked_idx < g_radial_menu.item_count) {
            RiceRadialItem item = g_radial_menu.items[clicked_idx];
            ricemain_radial_hide(dpy);

            if (item.action_id != 0 && g_radial_menu.action_cb) {
                g_radial_menu.action_cb(item.action_id, item.command, g_radial_menu.userdata);
            } else if (item.command[0] != '\0') {
                config_spawn_command(item.command);
            }
            return 1;
        }

        /* Clicked outside or center circle -> dismiss */
        ricemain_radial_hide(dpy);
        return 1;
    }

    if (ev->type == KeyPress) {
        KeySym sym = XLookupKeysym(&ev->xkey, 0);
        if (sym == XK_Escape) {
            ricemain_radial_hide(dpy);
            return 1;
        }
    }

    return 0;
}
