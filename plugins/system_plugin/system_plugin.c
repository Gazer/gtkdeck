#include "system_plugin.h"

#include "website.h"
#include "text.h"
#include "media.h"

DeckPlugin *system_plugin_clone(DeckPlugin *self, int action);
DeckPlugin *system_plugin_clone_with_code(DeckPlugin *self, int code);
DeckPluginInfo *system_plugin_info(DeckPlugin *self);

typedef struct _SystemPluginPrivate {
    gchar *url;
    int media_key;
} SystemPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(SystemPlugin, system_plugin, DECK_TYPE_PLUGIN)

typedef enum { URL = 1, MEDIA_KEY, N_PROPERTIES } SystemPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

typedef enum { WEBSITE = 0, TEXT, MULTIMEDIA, N_ACTIONS } SystemActionCodes;

DeckPluginInfo SYSTEM_PLUGIN_INFO = {
    "System",
    N_ACTIONS,
    {
        {BUTTON_MODE_NORMAL, "Website", "/ar/com/p39/gtkdeck/plugins/system-website.png", NULL,
         WEBSITE, website_config, website_exec, website_save, website_load},
        {BUTTON_MODE_NORMAL, "Text", "/ar/com/p39/gtkdeck/plugins/system-text.png", NULL, TEXT,
         text_config, text_exec, text_save, text_load},
        {BUTTON_MODE_NORMAL, "Multimedia", "/ar/com/p39/gtkdeck/plugins/system-previous.png", NULL,
         MULTIMEDIA, media_config, media_exec, media_save, media_load},
    },
};

static void system_plugin_init(SystemPlugin *self) {
    SystemPluginPrivate *priv = system_plugin_get_instance_private(self);

    // Set default values
    priv->url = NULL;
}

static void system_plugin_finalize(GObject *object) {
    SystemPlugin *self = SYSTEM_PLUGIN(object);
    SystemPluginPrivate *priv = system_plugin_get_instance_private(self);

    // Free memory if needed
    if (priv->url != NULL) {
        g_free(priv->url);
    }

    G_OBJECT_CLASS(system_plugin_parent_class)->finalize(object);
}

static void system_plugin_set_property(GObject *object, guint property_id, const GValue *value,
                                       GParamSpec *pspec) {
    SystemPlugin *self = SYSTEM_PLUGIN(object);
    SystemPluginPrivate *priv = system_plugin_get_instance_private(self);

    switch ((SystemPluginProperty)property_id) {
    case URL: {
        if (priv->url != NULL) {
            g_free(priv->url);
        }
        priv->url = g_strdup(g_value_get_string(value));
        break;
    }
    case MEDIA_KEY: {
        priv->media_key = g_value_get_uint(value);
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}
static void system_plugin_get_property(GObject *object, guint property_id, GValue *value,
                                       GParamSpec *pspec) {
    SystemPlugin *self = SYSTEM_PLUGIN(object);
    SystemPluginPrivate *priv = system_plugin_get_instance_private(self);

    switch ((SystemPluginProperty)property_id) {
    case URL: {
        g_value_set_string(value, priv->url);
        break;
    }
    case MEDIA_KEY: {
        g_value_set_uint(value, priv->media_key);
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void system_plugin_class_init(SystemPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    /* implement pure virtual function. */
    deck_plugin_klass->info = system_plugin_info;
    deck_plugin_klass->clone = system_plugin_clone;
    deck_plugin_klass->clone_with_code = system_plugin_clone_with_code;
    // deck_plugin_klass->save = system_plugin_save;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = system_plugin_set_property;
    object_class->get_property = system_plugin_get_property;

    object_class->finalize = system_plugin_finalize;

    obj_properties[URL] = g_param_spec_string("url", "Url", "Url.", NULL, G_PARAM_READWRITE);
    obj_properties[MEDIA_KEY] =
        g_param_spec_uint("media_key", "media_key", "media_key.", 0, 6, 0, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

DeckPlugin *system_plugin_new() {
    return g_object_new(SYSTEM_TYPE_PLUGIN, "name", "SystemPlugin", NULL);
}

DeckPlugin *system_plugin_clone(DeckPlugin *self, int action) {
    return g_object_new(SYSTEM_TYPE_PLUGIN, "name", "SystemPlugin", "action",
                        &SYSTEM_PLUGIN_INFO.actions[action], NULL);
}

DeckPluginInfo *system_plugin_info(DeckPlugin *self) { return &SYSTEM_PLUGIN_INFO; }

DeckPlugin *system_plugin_clone_with_code(DeckPlugin *self, int code) {
    for (int i = 0; i < N_ACTIONS; i++) {
        if (code == SYSTEM_PLUGIN_INFO.actions[i].code) {
            printf("  FOUND ACTION %i\n", i);
            return system_plugin_clone(self, i);
        }
    }

    return NULL;
}
