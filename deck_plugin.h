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

typedef struct _DeckPluginAction {
    gchar *name;
    int code;
    void (*config)(DeckPlugin *self, GtkBox *parent);
    void (*exec)(DeckPlugin *self);
} DeckPluginAction;

typedef struct _DeckPluginInfo {
    gchar *name;
    int actions_count;
    DeckPluginAction actions[];
} DeckPluginInfo;

struct _DeckPluginClass {
    GObjectClass parent;
    /* class members */
    DeckPluginInfo *(*info)(DeckPlugin *self);
    DeckPlugin *(*clone)(DeckPlugin *self, int action);
    void (*config_widget)(DeckPlugin *self, GtkBox *parent);
    void (*exec)(DeckPlugin *self);

    GThreadPool *pool;

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

/*
 * Method definitions.
 */
GType deck_plugin_get_type();
DeckPlugin *deck_plugin_new_with_action(DeckPlugin *self, int action);
DeckPluginInfo *deck_plugin_get_info(DeckPlugin *self);
void deck_plugin_get_config_widget(DeckPlugin *self, GtkBox *parent);
void deck_plugin_exec(DeckPlugin *self);
cairo_surface_t *deck_plugin_get_surface(DeckPlugin *self);
void deck_plugin_set_preview_from_file(DeckPlugin *self, char *filename);
void deck_plugin_reset(DeckPlugin *self);

G_END_DECLS

#endif /* __DECK_PLUGIN_H__ */