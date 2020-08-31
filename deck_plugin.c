#include "deck_plugin.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <libusb-1.0/libusb.h>

typedef struct _DeckPluginPrivate {
    gchar *name;
} DeckPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(DeckPlugin, deck_plugin, G_TYPE_OBJECT)

typedef enum { NAME = 1, N_PROPERTIES } DeckPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

static void deck_plugin_set_property(GObject *object, guint property_id, const GValue *value,
                                     GParamSpec *pspec) {
    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    switch ((DeckPluginProperty)property_id) {
    case NAME:
        priv->name = g_value_dup_string(value);
        printf("%p Name = %s\n", priv, priv->name);
        break;
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}
static void deck_plugin_get_property(GObject *object, guint property_id, GValue *value,
                                     GParamSpec *pspec) {}

static void deck_plugin_init(DeckPlugin *self) {}

static void deck_plugin_finalize(GObject *object) {
    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    g_free(priv->name);
}

static void deck_plugin_class_init(DeckPluginClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = deck_plugin_set_property;
    object_class->get_property = deck_plugin_get_property;
    // object_class->constructed = stream_deck_constructed;
    object_class->finalize = deck_plugin_finalize;

    obj_properties[NAME] =
        g_param_spec_string("name", "Name", "Name of the plugin, used to populate the plugin list.",
                            NULL, G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    // obj_properties[ROWS] = g_param_spec_int("rows", "Rows", "Rows.", 1, 10, 1,
    //                                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    // obj_properties[COLUMNS] = g_param_spec_int("columns", "Columns", "Columns.", 1, 10, 1,
    //                                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    // obj_properties[ICON_SIZE] = g_param_spec_int("icon_size", "Icon size", "Icon size.", 1, 100,
    // 1,
    //                                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    // obj_properties[KEY_DIRECTION] =
    //     g_param_spec_int("key_direction", "Key direction", "Key direction.", 0, 1, 0,
    //                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    // obj_properties[KEY_DATA_OFFSET] =
    //     g_param_spec_int("key_data_offset", "Key data offset", "Key data offset.", 1, G_MAXINT,
    //     1,
    //                      G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

const gchar *deck_plugin_get_name(DeckPlugin *plugin) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(plugin);

    printf("%p N4me = %s\n", priv, priv->name);
    return priv->name;
}

GtkWidget *deck_plugin_get_preview_widget(DeckPlugin *self) {
    DeckPluginClass *klass;
    g_return_val_if_fail(DECK_IS_PLUGIN(self), NULL);

    klass = DECK_PLUGIN_GET_CLASS(self);

    /* if the method is purely virtual, then it is a good idea to
     * check that it has been overridden before calling it, and,
     * depending on the intent of the class, either ignore it silently
     * or warn the user.
     */
    g_return_val_if_fail(klass->preview_widget != NULL, NULL);
    return klass->preview_widget(self);
}