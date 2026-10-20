#ifndef MUZU_COMPOSITOR_H
#define MUZU_COMPOSITOR_H

#include <X11/Xlib.h>

void compositor_init(Display *dpy);
void compositor_cleanup(void);
const char *compositor_get_config_path(void);

#endif /* MUZU_COMPOSITOR_H */
