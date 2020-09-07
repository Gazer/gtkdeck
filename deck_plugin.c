#include "deck_plugin.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <libusb-1.0/libusb.h>

typedef struct _DeckPluginPrivate {
    DeckPluginAction *action;
    cairo_surface_t *surface;
    cairo_surface_t *preview_image;
} DeckPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(DeckPlugin, deck_plugin, G_TYPE_OBJECT)

typedef enum { PREVIEW = 1, ACTION, N_PROPERTIES } DeckPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

void process_plugin_exec(gpointer data, gpointer user_data);

static void deck_plugin_set_property(GObject *object, guint property_id, const GValue *value,
                                     GParamSpec *pspec) {
    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    switch ((DeckPluginProperty)property_id) {
    case ACTION: {
        priv->action = g_value_get_pointer(value);
        break;
    }
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
                                     GParamSpec *pspec) {

    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    switch ((DeckPluginProperty)property_id) {
    case ACTION: {
        g_value_set_pointer(value, priv->action);
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void deck_plugin_init(DeckPlugin *self) {}

static void deck_plugin_finalize(GObject *object) {
    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    cairo_surface_destroy(priv->surface);
    if (priv->preview_image != NULL) {
        cairo_surface_destroy(priv->preview_image);
    }

    G_OBJECT_CLASS(deck_plugin_parent_class)->finalize(object);
    printf("Pluging finalized\n");
}

static void deck_plugin_class_init(DeckPluginClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = deck_plugin_set_property;
    object_class->get_property = deck_plugin_get_property;
    // object_class->constructed = stream_deck_constructed;
    object_class->finalize = deck_plugin_finalize;

    klass->pool = g_thread_pool_new(process_plugin_exec, NULL, 5, TRUE, NULL);

    obj_properties[ACTION] =
        g_param_spec_pointer("action", "Action", "Plugin action to execute.", G_PARAM_READWRITE);

    obj_properties[PREVIEW] =
        g_param_spec_pointer("preview", "Preview", "Preview image to show.", G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

DeckPluginInfo *deck_plugin_get_info(DeckPlugin *self) {
    DeckPluginClass *klass;
    g_return_val_if_fail(DECK_IS_PLUGIN(self), NULL);

    klass = DECK_PLUGIN_GET_CLASS(self);

    /* if the method is purely virtual, then it is a good idea to
     * check that it has been overridden before calling it, and,
     * depending on the intent of the class, either ignore it silently
     * or warn the user.
     */
    g_return_val_if_fail(klass->info != NULL, NULL);
    return klass->info(self);
}

DeckPlugin *deck_plugin_new_with_action(DeckPlugin *self, int action) {
    DeckPluginClass *klass;
    g_return_val_if_fail(DECK_IS_PLUGIN(self), NULL);

    klass = DECK_PLUGIN_GET_CLASS(self);

    /* if the method is purely virtual, then it is a good idea to
     * check that it has been overridden before calling it, and,
     * depending on the intent of the class, either ignore it silently
     * or warn the user.
     */
    g_return_val_if_fail(klass->clone != NULL, NULL);
    return klass->clone(self, action);
}

void process_plugin_exec(gpointer data, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(data);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    priv->action->exec(data);

    deck_plugin_reset(self);
}

void deck_plugin_exec(DeckPlugin *self) {
    DeckPluginClass *klass = DECK_PLUGIN_GET_CLASS(self);
    g_thread_pool_push(klass->pool, self, NULL);
}

void deck_plugin_get_config_widget(DeckPlugin *self, GtkBox *parent) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    priv->action->config(self, parent);
}

cairo_surface_t *deck_plugin_get_surface(DeckPlugin *self) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    return priv->surface;
}

void deck_plugin_set_preview_from_file(DeckPlugin *self, char *filename) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(filename, 72, 72, TRUE, NULL);

    if (priv->preview_image != NULL) {
        cairo_surface_destroy(priv->preview_image);
    }
    priv->preview_image = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);

    g_object_set(G_OBJECT(self), "preview", priv->preview_image, NULL);
}

void deck_plugin_reset(DeckPlugin *self) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    g_object_set(G_OBJECT(self), "preview", priv->preview_image, NULL);
}