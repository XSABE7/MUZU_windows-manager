#ifndef MUZU_CONFIRM_DIALOG_H
#define MUZU_CONFIRM_DIALOG_H

#include <X11/Xlib.h>
#include <cairo/cairo.h>
#include "gtk_theme.h"

#define CONFIRM_DIALOG_WIDTH 360
#define CONFIRM_DIALOG_HEIGHT 150
#define CONFIRM_DIALOG_PAD 16
#define CONFIRM_DIALOG_BTN_H 30
#define CONFIRM_DIALOG_BTN_GAP 12

typedef enum {
    CONFIRM_BTN_NONE = 0,
    CONFIRM_BTN_CANCEL = 1,
    CONFIRM_BTN_OK = 2
} ConfirmButton;

typedef struct {
    Display *dpy;
    Window win;
    cairo_surface_t *surface;
    Visual *visual;
    int depth;
    Colormap colormap;
    int is_visible;
    int x, y, width, height;
    int action_id;
    int hover_btn;
    char title[64];
    char message[128];
    char confirm_label[32];
    const GtkTheme *theme;
} ConfirmDialog;

/* Returns 1 for power actions that must be confirmed before running */
int power_action_needs_confirm(int action_id);

void confirm_dialog_init(Display *dpy, ConfirmDialog *d, const GtkTheme *theme);
/* Fill texts and centered geometry for action_id (no X calls) */
void confirm_dialog_prepare(ConfirmDialog *d, int action_id, int screen_w, int screen_h);
void confirm_dialog_button_rect(const ConfirmDialog *d, ConfirmButton btn,
                                int *x, int *y, int *w, int *h);
ConfirmButton confirm_dialog_button_at(const ConfirmDialog *d, int x, int y);
/* Returns 1 if hover state changed (re-renders when dpy is set) */
int confirm_dialog_handle_motion(Display *dpy, ConfirmDialog *d, int x, int y);
void confirm_dialog_show(Display *dpy, ConfirmDialog *d, int action_id, int screen_w, int screen_h);
void confirm_dialog_hide(Display *dpy, ConfirmDialog *d);
void confirm_dialog_render(Display *dpy, ConfirmDialog *d);
void confirm_dialog_cleanup(ConfirmDialog *d);

#endif /* MUZU_CONFIRM_DIALOG_H */
