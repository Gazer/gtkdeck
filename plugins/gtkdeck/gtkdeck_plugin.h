#ifndef __GTKDECK_PLUGIN_H__
#define __GTKDECK_PLUGIN_H__

#include "../deck_plugin.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define GTKDECK_TYPE_PLUGIN (gtkdeck_plugin_get_type())

G_DECLARE_FINAL_TYPE(GtkDeckPlugin, gtkdeck_plugin, GTKDECK, PLUGIN, DeckPlugin)

DeckPlugin *gtkdeck_plugin_new();

G_END_DECLS

#endif
