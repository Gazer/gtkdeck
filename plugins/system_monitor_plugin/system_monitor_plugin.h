#include "../deck_plugin.h"

#include <gmodule.h>
#include <gtk/gtk.h>

#ifndef __SYSTEM_MONITOR_PLUGIN_H__
#define __SYSTEM_MONITOR_PLUGIN_H__

G_BEGIN_DECLS

#define SYSTEM_MONITOR_TYPE_PLUGIN system_monitor_plugin_get_type()
G_DECLARE_FINAL_TYPE(SystemMonitorPlugin, system_monitor_plugin, SYSTEM_MONITOR, PLUGIN, DeckPlugin)

struct _SystemMonitorPluginClass {
    DeckPluginClass parent;
    gpointer padding[12];
};

struct _SystemMonitorPlugin {
    DeckPlugin parent;
};

DeckPlugin *system_monitor_plugin_new();

G_END_DECLS

#endif /* __SYSTEM_MONITOR_PLUGIN_H__ */
