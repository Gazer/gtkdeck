#include "deck_plugin.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <libusb-1.0/libusb.h>

typedef struct _DeckPluginPrivate {
    gchar *name;
    cairo_surface_t *surface;
} DeckPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(DeckPlugin, deck_plugin, G_TYPE_OBJECT)

typedef enum { NAME = 1, PREVIEW, N_PROPERTIES } DeckPluginProperty;

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
        break;
    case PREVIEW: {
        priv->surface = g_value_get_pointer(value);
        break;
    }
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
    cairo_surface_destroy(priv->surface);

    G_OBJECT_CLASS(deck_plugin_parent_class)->finalize(object);
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
    obj_properties[PREVIEW] =
        g_param_spec_pointer("preview", "Preview", "Preview image to show.", G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

const gchar *deck_plugin_get_name(DeckPlugin *plugin) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(plugin);

    return priv->name;
}

DeckPlugin *deck_plugin_clone(DeckPlugin *self) {
    DeckPluginClass *klass;
    g_return_val_if_fail(DECK_IS_PLUGIN(self), NULL);

    klass = DECK_PLUGIN_GET_CLASS(self);

    /* if the method is purely virtual, then it is a good idea to
     * check that it has been overridden before calling it, and,
     * depending on the intent of the class, either ignore it silently
     * or warn the user.
     */
    g_return_val_if_fail(klass->clone != NULL, NULL);
    return klass->clone(self);
}

GtkWidget *deck_plugin_get_preview_widget(DeckPlugin *self) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    GtkWidget *preview = gtk_image_new_from_surface(priv->surface);

    return preview;
}

void deck_plugin_get_config_widget(DeckPlugin *self, GtkBox *parent) {
    DeckPluginClass *klass;
    g_return_if_fail(DECK_IS_PLUGIN(self));

    klass = DECK_PLUGIN_GET_CLASS(self);

    /* if the method is purely virtual, then it is a good idea to
     * check that it has been overridden before calling it, and,
     * depending on the intent of the class, either ignore it silently
     * or warn the user.
     */
    g_return_if_fail(klass->config_widget != NULL);
    printf("call virtual\n");
    return klass->config_widget(self, parent);
}
