#include "deck_plugin.h"

#include <gmodule.h>
#include <gtk/gtk.h>
#include <libusb-1.0/libusb.h>
#include <stdio.h>

#ifndef __TEST_PLUGIN_H__
#define __TEST_PLUGIN_H__

G_BEGIN_DECLS

/*
 * Type declaration.
 */
#define TEST_TYPE_PLUGIN test_plugin_get_type()
G_DECLARE_FINAL_TYPE(TestPlugin, test_plugin, TEST, PLUGIN, DeckPlugin)

struct _TestluginClass {
    DeckPluginClass parent;
    /* class members */
    // void (*do_action_public_virtual)(MamanBar *self, guint8 i);
    // void (*do_action_public_pure_virtual)(MamanBar *self, guint8 i);

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

struct _TestPlugin {
    DeckPlugin parent;
};

DeckPlugin *test_plugin_new();

G_END_DECLS

#endif /* __TEST_PLUGIN_H__ */