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

typedef enum _DeckPluginButtonMode {
    BUTTON_MODE_NORMAL,
    BUTTON_MODE_TOGGLE,
} DeckPluginButtonMode;

typedef enum _DeckPluginState {
    BUTTON_STATE_NORMAL,
    BUTTON_STATE_SELECTED,
} DeckPluginState;

typedef struct _DeckPluginAction {
    DeckPluginButtonMode mode;
    gchar *name;
    const gchar *default_image;
    const gchar *selected_image;
    int code;
    void (*config)(DeckPlugin *self, GtkBox *parent);
    void (*exec)(DeckPlugin *self);
    void (*save)(DeckPlugin *self, char *group, GKeyFile *key_file);
    void (*load)(DeckPlugin *self, char *group, GKeyFile *key_file);
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
    DeckPlugin *(*clone_with_code)(DeckPlugin *self, int code);
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
GList *deck_plugin_list();
void deck_plugin_exit();
void deck_plugin_register(DeckPlugin *plugin);
void deck_plugin_save(DeckPlugin *self, int position, GKeyFile *key_file);
DeckPlugin *deck_plugin_load(GKeyFile *key_file, char *group);
DeckPlugin *deck_plugin_new_with_action(DeckPlugin *self, int action);
DeckPlugin *deck_plugin_new_with_action_code(DeckPlugin *self, int code);
DeckPluginInfo *deck_plugin_get_info(DeckPlugin *self);
void deck_plugin_get_config_widget(DeckPlugin *self, GtkBox *parent);
void deck_plugin_exec(DeckPlugin *self);
void deck_plugin_toggle(DeckPlugin *self);
cairo_surface_t *deck_plugin_get_surface(DeckPlugin *self);
cairo_surface_t *deck_plugin_get_image(DeckPlugin *self, DeckPluginState mode);
void deck_plugin_set_image_from_file(DeckPlugin *self, DeckPluginState mode, char *filename);
void deck_plugin_set_image_from_resource(DeckPlugin *self, DeckPluginState mode, char *resource);
void deck_plugin_reset(DeckPlugin *self);
DeckPluginButtonMode deck_plugin_get_button_mode(DeckPlugin *self);

G_END_DECLS

#endif /* __DECK_PLUGIN_H__ */