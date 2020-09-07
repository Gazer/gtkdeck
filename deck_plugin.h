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
G_DECLARE_DERIVABLE_TYPE(DeckPlugin, deck_plugin, DECK, PLUGIN, GObject)

struct _DeckPluginClass {
    GObjectClass parent;
    /* class members */
    DeckPlugin *(*clone)(DeckPlugin *self);
    void (*config_widget)(DeckPlugin *self, GtkBox *parent);
    void (*exec)(DeckPlugin *self);

    GThreadPool *pool;

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
DeckPlugin *deck_plugin_clone(DeckPlugin *self);
const gchar *deck_plugin_get_name(DeckPlugin *self);
GtkWidget *deck_plugin_get_preview_widget(DeckPlugin *self);
void deck_plugin_get_config_widget(DeckPlugin *self, GtkBox *parent);
void deck_plugin_exec(DeckPlugin *self);
cairo_surface_t *deck_plugin_get_surface(DeckPlugin *self);
void deck_plugin_set_preview_from_file(DeckPlugin *self, char *filename);
void deck_plugin_reset(DeckPlugin *self);

G_END_DECLS

#endif /* __DECK_PLUGIN_H__ */