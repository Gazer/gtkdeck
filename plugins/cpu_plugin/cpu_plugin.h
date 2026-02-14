#include "../deck_plugin.h"

#include <gmodule.h>
#include <gtk/gtk.h>

#ifndef __CPU_PLUGIN_H__
#define __CPU_PLUGIN_H__

G_BEGIN_DECLS

#define CPU_TYPE_PLUGIN cpu_plugin_get_type()
G_DECLARE_FINAL_TYPE(CpuPlugin, cpu_plugin, CPU, PLUGIN, DeckPlugin)

struct _CpuPluginClass {
    DeckPluginClass parent;
    gpointer padding[12];
};

struct _CpuPlugin {
    DeckPlugin parent;
};

DeckPlugin *cpu_plugin_new();

G_END_DECLS

#endif /* __CPU_PLUGIN_H__ */
