/* inclusion guard */

#include <glib-object.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <libusb-1.0/libusb.h>
#include <stdio.h>

#ifndef __DECK_PLUGIN_H__
#define __DECK_PLUGIN_H__

G_BEGIN_DECLS

/*
 * Type declaration.
 */
#define DECK_TYPE_PLUGIN deck_plugin_get_type()
GLIB_AVAILABLE_IN_2_32
G_DECLARE_DERIVABLE_TYPE(DeckPlugin, deck_plugin, DECK, PLUGIN, GObject)

struct _DeckPluginClass {
    GObjectClass parent;
    /* class members */
    GtkWidget *(*preview_widget)(DeckPlugin *self);
    // void (*do_action_public_pure_virtual)(MamanBar *self, guint8 i);

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

// struct _DeckPlugin {
//     GObject parent;
// };

/*
 * Method definitions.
 */
GType deck_plugin_get_type();
const gchar *deck_plugin_get_name(DeckPlugin *plugin);
GtkWidget *deck_plugin_get_preview_widget(DeckPlugin *plugin);

G_END_DECLS

#endif /* __DECK_PLUGIN_H__ */