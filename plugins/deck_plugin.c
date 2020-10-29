#include "deck_plugin.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <libusb-1.0/libusb.h>
#include <pango/pangocairo.h>

// Plugins
#include "test_plugin.h"
#include "system_plugin/system_plugin.h"
#include "obs_plugin/obs_plugin.h"

static GList *available_plugins = NULL;

typedef struct _DeckPluginPrivate {
    DeckPluginAction *action;
    char *name;

    char *label;
    cairo_surface_t *surface;
    cairo_surface_t *preview_image;
    cairo_surface_t *preview_image_active;
    DeckPluginState state;
} DeckPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(DeckPlugin, deck_plugin, G_TYPE_OBJECT)

typedef enum { PREVIEW = 1, NAME, ACTION, LABEL, N_PROPERTIES } DeckPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

void process_plugin_exec(gpointer data, gpointer user_data);

static void deck_plugin_set_property(GObject *object, guint property_id, const GValue *value,
                                     GParamSpec *pspec) {
    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    GdkPixbuf *pixbuf = NULL;

    switch ((DeckPluginProperty)property_id) {
    case ACTION: {
        priv->action = g_value_get_pointer(value);
        printf(">> Action set\n");

        if (priv->action != NULL && priv->action->default_image != NULL) {
            pixbuf = gdk_pixbuf_new_from_resource_at_scale(priv->action->default_image, 300, 300,
                                                           TRUE, NULL);
        } else {
            pixbuf = gdk_pixbuf_new_from_resource_at_scale("/ar/com/p39/gtkdeck/generic_icon.png",
                                                           300, 300, TRUE, NULL);
        }
        if (pixbuf != NULL) {
            priv->preview_image = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);
            g_object_unref(pixbuf);
        }
        if (priv->action != NULL && priv->action->selected_image != NULL) {
            pixbuf = gdk_pixbuf_new_from_resource_at_scale(priv->action->selected_image, 300, 300,
                                                           TRUE, NULL);
            if (pixbuf != NULL) {
                priv->preview_image_active = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);
                g_object_unref(pixbuf);
            }
        }

        deck_plugin_reset(self);
        break;
    }
    case NAME: {
        if (priv->name != NULL) {
            g_free(priv->name);
        }
        priv->name = g_strdup(g_value_get_string(value));
        break;
    }
    case LABEL: {
        if (priv->label != NULL) {
            g_free(priv->label);
        }
        priv->label = g_value_dup_string(value);
        deck_plugin_reset(self);
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
    case LABEL: {
        g_value_set_string(value, priv->label);
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
    priv->preview_image = NULL;
    priv->preview_image_active = NULL;
    priv->label = NULL;
}

static void deck_plugin_finalize(GObject *object) {
    DeckPlugin *self = DECK_PLUGIN(object);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    if (priv->preview_image != NULL) {
        cairo_surface_destroy(priv->preview_image);
    }
    if (priv->preview_image_active != NULL) {
        cairo_surface_destroy(priv->preview_image_active);
    }
    if (priv->name != NULL) {
        g_free(priv->name);
    }
    if (priv->label != NULL) {
        g_free(priv->label);
    }

    G_OBJECT_CLASS(deck_plugin_parent_class)->finalize(object);
    printf("Pluging finalized\n");
}

static void deck_plugin_constructed(GObject *object) {
    // DeckPlugin *self = DECK_PLUGIN(object);
    // DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    // deck_plugin_reset(self);
}

static void deck_plugin_class_init(DeckPluginClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = deck_plugin_set_property;
    object_class->get_property = deck_plugin_get_property;
    object_class->constructed = deck_plugin_constructed;
    object_class->finalize = deck_plugin_finalize;

    klass->pool = g_thread_pool_new(process_plugin_exec, NULL, 5, TRUE, NULL);
    klass->render = NULL;

    obj_properties[ACTION] =
        g_param_spec_pointer("action", "Action", "Plugin action to execute.", G_PARAM_READWRITE);

    obj_properties[NAME] =
        g_param_spec_string("name", "Name", "Name of the plugin.", NULL, G_PARAM_READWRITE);

    obj_properties[LABEL] =
        g_param_spec_string("label", "Label", "Label of the plugin.", NULL, G_PARAM_READWRITE);

    obj_properties[PREVIEW] =
        g_param_spec_pointer("preview", "Preview", "Preview image to show.", G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void deck_plugin_set_current_state(DeckPlugin *self) {
    DeckPluginClass *klass = DECK_PLUGIN_GET_CLASS(self);
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    cairo_surface_t *surface;

    if (priv->state == BUTTON_STATE_SELECTED) {
        surface = priv->preview_image_active;
    } else {
        surface = priv->preview_image, NULL;
    }

    if (priv->label != NULL && strlen(priv->label) > 0) {
        int width, height;
        cairo_surface_t *new_surface =
            cairo_surface_create_similar(surface, CAIRO_CONTENT_COLOR_ALPHA, 300, 300);

        cairo_t *cr = cairo_create(new_surface);
        // Paint the base image
        cairo_set_source_surface(cr, surface, 0, 0);
        cairo_paint(cr);

        /* Create a PangoLayout, set the font and text */
        PangoLayout *layout = pango_cairo_create_layout(cr);

        pango_layout_set_text(layout, priv->label, -1);
        PangoFontDescription *desc = pango_font_description_from_string("Sans Bold 40");
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        cairo_save(cr);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        pango_cairo_update_layout(cr, layout);
        pango_layout_get_size(layout, &width, &height);
        cairo_move_to(cr, 300 / 2 - width / PANGO_SCALE / 2, 300 - height / PANGO_SCALE);
        pango_cairo_show_layout(cr, layout);
        cairo_restore(cr);

        if (klass->render != NULL) {
            klass->render(self, cr, 300, 300);
        }

        // Cleanup
        cairo_destroy(cr);

        surface = new_surface;
    }

    g_object_set(G_OBJECT(self), "preview", surface, NULL);
}

GList *deck_plugin_list() {
    if (available_plugins == NULL) {
        // Register plugins
        deck_plugin_register(test_plugin_new());
        deck_plugin_register(system_plugin_new());
        deck_plugin_register(obs_plugin_new());
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

void label_text_changed(GtkEditable *editable, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);
    char *text = gtk_editable_get_chars(editable, 0, -1);
    g_object_set(G_OBJECT(self), "label", text, NULL);
    g_free(text);
}

void deck_plugin_get_config_widget(DeckPlugin *self, GtkBox *parent) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    gchar *text;
    g_object_get(G_OBJECT(self), "label", &text, NULL);

    GtkBox *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label = gtk_label_new("Label");
    GtkWidget *entry = gtk_entry_new();
    if (text != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry), text);
    }

    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(label_text_changed), self);

    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 5);

    gtk_box_pack_start(parent, GTK_WIDGET(hbox), TRUE, FALSE, 5);

    priv->action->config(self, parent);
}

cairo_surface_t *deck_plugin_get_surface(DeckPlugin *self) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    return priv->surface;
}

cairo_surface_t *deck_plugin_get_image(DeckPlugin *self, DeckPluginState mode) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    if (mode == BUTTON_STATE_NORMAL) {
        return priv->preview_image;
    }

    return priv->preview_image_active;
}

void deck_plugin_set_image_from_file(DeckPlugin *self, DeckPluginState mode, char *filename) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_size(filename, 300, 300, NULL);

    if (mode == BUTTON_STATE_NORMAL) {
        if (priv->preview_image != NULL) {
            cairo_surface_destroy(priv->preview_image);
        }
        priv->preview_image = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);

    } else {
        if (priv->preview_image_active != NULL) {
            cairo_surface_destroy(priv->preview_image_active);
        }
        priv->preview_image_active = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);
    }
    deck_plugin_set_current_state(self);
}

void deck_plugin_set_image_from_resource(DeckPlugin *self, DeckPluginState mode, char *resource) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource(resource, NULL);
    if (pixbuf != NULL) {
        if (mode == BUTTON_STATE_NORMAL) {
            priv->preview_image = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);
        } else {
            priv->preview_image_active = gdk_cairo_surface_create_from_pixbuf(pixbuf, 0, NULL);
        }
        g_object_unref(pixbuf);
    }
    deck_plugin_set_current_state(self);
}

void deck_plugin_reset(DeckPlugin *self) { deck_plugin_set_current_state(self); }

void deck_plugin_toggle(DeckPlugin *self) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    if (priv->state == BUTTON_STATE_NORMAL) {
        priv->state = BUTTON_STATE_SELECTED;
    } else {
        priv->state = BUTTON_STATE_NORMAL;
    }
}

cairo_surface_t *g_key_file_get_surface(GKeyFile *key_file, char *group, char *prefix) {
    char *data;
    cairo_format_t format;
    int width, height, stride;
    gsize data_size;

    g_autofree char *l_data = g_strdup_printf("%s_data", prefix);
    g_autofree char *l_width = g_strdup_printf("%s_width", prefix);
    g_autofree char *l_height = g_strdup_printf("%s_height", prefix);
    g_autofree char *l_stride = g_strdup_printf("%s_stride", prefix);
    g_autofree char *l_format = g_strdup_printf("%s_format", prefix);

    data = g_key_file_get_value(key_file, (const gchar *)group, (const gchar *)l_data, NULL);
    unsigned char *surface = g_base64_decode((const gchar *)data, &data_size);
    g_free(data);

    height = g_key_file_get_integer(key_file, group, l_height, NULL);
    stride = g_key_file_get_integer(key_file, group, l_stride, NULL);
    format = g_key_file_get_integer(key_file, group, l_format, NULL);
    width = g_key_file_get_integer(key_file, group, l_width, NULL);

    return cairo_image_surface_create_for_data(surface, format, width, height, stride);
}

void g_key_file_set_surface(GKeyFile *key_file, char *group, char *prefix,
                            cairo_surface_t *surface) {
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
    stride = cairo_image_surface_get_stride(surface);
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

    // Save
    g_autofree char *l_data = g_strdup_printf("%s_data", prefix);
    g_autofree char *base64 = g_base64_encode(data, data_size);
    g_key_file_set_string(key_file, group, l_data, base64);

    g_autofree char *l_width = g_strdup_printf("%s_width", prefix);
    g_key_file_set_integer(key_file, group, l_width, width);

    g_autofree char *l_height = g_strdup_printf("%s_height", prefix);
    g_key_file_set_integer(key_file, group, l_height, height);

    g_autofree char *l_stride = g_strdup_printf("%s_stride", prefix);
    g_key_file_set_integer(key_file, group, l_stride, stride);

    g_autofree char *l_format = g_strdup_printf("%s_format", prefix);
    g_key_file_set_integer(key_file, group, l_format, format);
}

void deck_plugin_save(DeckPlugin *self, int position, GKeyFile *key_file) {
    g_return_if_fail(DECK_IS_PLUGIN(self));
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    g_autofree char *group = g_strdup_printf("key_%d", position);

    g_key_file_set_string(key_file, group, "name", priv->name);
    g_key_file_set_string(key_file, group, "action_name", priv->action->name);
    g_key_file_set_integer(key_file, group, "code", priv->action->code);

    if (priv->preview_image != NULL) {
        g_key_file_set_surface(key_file, group, "preview_image", priv->preview_image);
    }
    if (priv->preview_image_active != NULL) {
        g_key_file_set_surface(key_file, group, "preview_image_active", priv->preview_image_active);
    }
    if (priv->label != NULL) {
        printf("Saving label %s\n", priv->label);
        g_key_file_set_string(key_file, group, "label", priv->label);
    }

    priv->action->save(self, group, key_file);
}

DeckPlugin *deck_plugin_load(GKeyFile *key_file, char *group) {
    GList *list = deck_plugin_list();

    g_autofree char *name = g_key_file_get_string(key_file, group, "name", NULL);
    g_autofree char *label = g_key_file_get_string(key_file, group, "label", NULL);
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

            DeckPlugin *new_plugin = deck_plugin_new_with_action_code(plugin, code);
            DeckPluginPrivate *new_priv = deck_plugin_get_instance_private(new_plugin);

            if (label != NULL) {
                new_priv->label = g_strdup(label);
            }

            if (g_key_file_has_key(key_file, group, "preview_image_data", NULL)) {
                new_priv->preview_image = g_key_file_get_surface(key_file, group, "preview_image");
            }
            if (g_key_file_has_key(key_file, group, "preview_image_active_data", NULL)) {
                new_priv->preview_image_active =
                    g_key_file_get_surface(key_file, group, "preview_image_active");
            }
            new_priv->action->load(new_plugin, group, key_file);

            deck_plugin_reset(new_plugin);
            return new_plugin;
        }
        list = list->next;
    }
    return NULL;
}

DeckPluginButtonMode deck_plugin_get_button_mode(DeckPlugin *self) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);

    return priv->action->mode;
}

void deck_plugin_set_state(DeckPlugin *self, DeckPluginState state) {
    DeckPluginPrivate *priv = deck_plugin_get_instance_private(self);
    priv->state = state;

    deck_plugin_reset(self);
}