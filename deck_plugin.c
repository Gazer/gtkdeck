#include "deck_plugin.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <libusb-1.0/libusb.h>

// Plugins
#include "test_plugin.h"
#include "system_plugin/system_plugin.h"

static GList *available_plugins = NULL;

typedef struct _DeckPluginPrivate {
    DeckPluginAction *action;
    char *name;
    cairo_surface_t *surface;
    cairo_surface_t *preview_image;
} DeckPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(DeckPlugin, deck_plugin, G_TYPE_OBJECT)

typedef enum { PREVIEW = 1, NAME, ACTION, N_PROPERTIES } DeckPluginProperty;

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
    case NAME: {
        if (priv->name != NULL) {
            g_free(priv->name);
        }
        priv->name = g_strdup(g_value_get_string(value));
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

static void deck_plugin_init(DeckPlugin *self) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    priv->name = NULL;
}

static void deck_plugin_finalize(GObject *object) {
    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    cairo_surface_destroy(priv->surface);
    if (priv->preview_image != NULL) {
        cairo_surface_destroy(priv->preview_image);
    }
    if (priv->name != NULL) {
        g_free(priv->name);
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

    obj_properties[NAME] =
        g_param_spec_string("name", "Name", "Name of the plugin.", NULL, G_PARAM_READWRITE);

    obj_properties[PREVIEW] =
        g_param_spec_pointer("preview", "Preview", "Preview image to show.", G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

GList *deck_plugin_list() {
    if (available_plugins == NULL) {
        // Register plugins
        deck_plugin_register(test_plugin_new());
        deck_plugin_register(system_plugin_new());
    }
    return available_plugins;
}

void deck_plugin_register(DeckPlugin *plugin) {
    available_plugins = g_list_append(available_plugins, plugin);
}

void deck_plugin_exit() { g_list_free(available_plugins); }

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

DeckPlugin *deck_plugin_new_with_action_code(DeckPlugin *self, int code) {
    DeckPluginClass *klass;
    g_return_val_if_fail(DECK_IS_PLUGIN(self), NULL);

    klass = DECK_PLUGIN_GET_CLASS(self);

    /* if the method is purely virtual, then it is a good idea to
     * check that it has been overridden before calling it, and,
     * depending on the intent of the class, either ignore it silently
     * or warn the user.
     */
    g_return_val_if_fail(klass->clone_with_code != NULL, NULL);
    return klass->clone_with_code(self, code);
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

char *surface_to_base64(cairo_surface_t *surface) {
    unsigned char *data;
    cairo_format_t format;
    int width, height, stride;
    int data_size;

    // flush to ensure all writing to the image was done
    cairo_surface_flush(surface);

    // modify the image
    data = cairo_image_surface_get_data(surface);
    width = cairo_image_surface_get_width(surface);
    height = cairo_image_surface_get_height(surface);
    format = cairo_image_surface_get_format(surface);
    data_size = width * height;

    // We need bytes, data_size now has "pixels"
    if (format == CAIRO_FORMAT_ARGB32) {
        data_size *= 4;
    } else if (format == CAIRO_FORMAT_RGB24) {
        data_size *= 4;
    } else if (format == CAIRO_FORMAT_RGB16_565) {
        data_size *= 2;
    } else if (format == CAIRO_FORMAT_RGB30) {
        data_size *= 4;
    }

    return g_base64_encode(data, data_size);
}

void deck_plugin_save(DeckPlugin *self, int position, GKeyFile *key_file) {
    g_return_if_fail(DECK_IS_PLUGIN(self));
    DeckPluginClass *klass;
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    klass = DECK_PLUGIN_GET_CLASS(self);

    g_autofree char *group = g_strdup_printf("key_%d", position);

    g_key_file_set_string(key_file, group, "name", priv->name);
    g_key_file_set_string(key_file, group, "action_name", priv->action->name);
    g_key_file_set_integer(key_file, group, "code", priv->action->code);

    if (priv->preview_image != NULL) {
        g_autofree char *preview_image = surface_to_base64(priv->preview_image);
        g_key_file_set_string(key_file, group, "preview_image", preview_image);
    }

    /* if the method is purely virtual, then it is a good idea to
     * check that it has been overridden before calling it, and,
     * depending on the intent of the class, either ignore it silently
     * or warn the user.
     */
    g_return_if_fail(klass->save != NULL);

    // Save plugin settings
    klass->save(self, position, key_file);
}

DeckPlugin *deck_plugin_load(GKeyFile *key_file, const char *group) {
    GList *list = deck_plugin_list();

    g_autofree char *name = g_key_file_get_string(key_file, group, "name", NULL);
    g_autofree char *action_name = g_key_file_get_string(key_file, group, "action_name", NULL);
    int code = g_key_file_get_integer(key_file, group, "code", NULL);

    printf("  Name      : %s\n", name);
    printf("  ActionName: %s\n", action_name);
    printf("  Code      : %d\n", code);

    while (list != NULL) {
        DeckPlugin *plugin = DECK_PLUGIN(list->data);
        DeckPluginPrivate *priv = deck_plugin_get_instance_private(plugin);

        if (g_strcmp0(name, priv->name) == 0) {
            printf("  FOUND PLUGIN!\n");
            return deck_plugin_new_with_action_code(plugin, code);
        }
        list = list->next;
    }
    return NULL;
}