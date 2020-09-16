#include "../deck_plugin.h"

#include <gmodule.h>
#include <gtk/gtk.h>
#include <stdio.h>

#ifndef __SYSTEM_PLUGIN_H__
#define __SYSTEM_PLUGIN_H__

G_BEGIN_DECLS

/*
 * Type declaration.
 */
#define SYSTEM_TYPE_PLUGIN system_plugin_get_type()
G_DECLARE_FINAL_TYPE(SystemPlugin, system_plugin, SYSTEM, PLUGIN, DeckPlugin)

struct _SystemluginClass {
    DeckPluginClass parent;
    /* class members */
    // void (*do_action_public_virtual)(MamanBar *self, guint8 i);
    // void (*do_action_public_pure_virtual)(MamanBar *self, guint8 i);

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

struct _SystemPlugin {
    DeckPlugin parent;
};

DeckPlugin *system_plugin_new();

G_END_DECLS

#endif /* __SYSTEM_PLUGIN_H__ */