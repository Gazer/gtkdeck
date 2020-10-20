#include "../deck_plugin.h"

#include <gmodule.h>
#include <gtk/gtk.h>
#include <stdio.h>

#ifndef __OBS_PLUGIN_H__
#define __OBS_PLUGIN_H__

G_BEGIN_DECLS

/*
 * Type declaration.
 */
#define OBS_TYPE_PLUGIN obs_plugin_get_type()
G_DECLARE_FINAL_TYPE(OBSPlugin, obs_plugin, OBS, PLUGIN, DeckPlugin)

struct _OBSPluginClass {
    DeckPluginClass parent;
    /* class members */
    // void (*do_action_public_virtual)(MamanBar *self, guint8 i);
    // void (*do_action_public_pure_virtual)(MamanBar *self, guint8 i);

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

struct _OBSPlugin {
    DeckPlugin parent;
};

DeckPlugin *obs_plugin_new();

G_END_DECLS

#endif /* __OBS_PLUGIN_H__ */