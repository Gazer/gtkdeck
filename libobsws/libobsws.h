#include <gtk/gtk.h>

#ifndef __OBS_WS_H__
#define __OBS_WS_H__

G_BEGIN_DECLS

#define OBS_TYPE_WS obs_ws_get_type()
G_DECLARE_DERIVABLE_TYPE(ObsWs, obs_ws, OBS, WS, GObject)

struct _ObsWsClass {
    GObjectClass parent;

    /* class members */
    void (*heartbeat)(const gchar *profile, const gchar *scene);

    GThread *thread;

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

/*
 * Method definitions.
 */
GType obs_ws_get_type();
ObsWs *obs_ws_new();

G_END_DECLS

#endif // __OBS_WS_H__
