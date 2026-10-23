#ifndef MUZU_COMPOSITOR_H
#define MUZU_COMPOSITOR_H

#include <stddef.h>
#include <X11/Xlib.h>

void compositor_init(Display *dpy);
void compositor_cleanup(void);
const char *compositor_get_config_path(void);
int compositor_find_picom(char *out_bin, size_t out_len);

#endif /* MUZU_COMPOSITOR_H */
