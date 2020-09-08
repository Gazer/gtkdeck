#include "system_plugin.h"

#include "website.h"

DeckPlugin *system_plugin_clone(DeckPlugin *self, int action);
DeckPluginInfo *system_plugin_info(DeckPlugin *self);

typedef struct _SystemPluginPrivate {
    gchar *url;
} SystemPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(SystemPlugin, system_plugin, DECK_TYPE_PLUGIN)

typedef enum { URL = 1, N_PROPERTIES } SystemPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

typedef enum { WEBSITE = 0, N_ACTIONS } SystemActionCodes;

DeckPluginInfo SYSTEM_PLUGIN_INFO = {
    "System",
    N_ACTIONS,
    {
        {"Website", WEBSITE, website_config, website_exec},
        // {"Always Red", RED_BUTTON, test_1_config, test_1_exec},
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

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = system_plugin_set_property;
    object_class->get_property = system_plugin_get_property;

    object_class->finalize = system_plugin_finalize;

    obj_properties[URL] = g_param_spec_string("url", "Url", "Url.", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

DeckPlugin *system_plugin_new() { return g_object_new(SYSTEM_TYPE_PLUGIN, NULL); }

DeckPlugin *system_plugin_clone(DeckPlugin *self, int action) {
    return g_object_new(SYSTEM_TYPE_PLUGIN, "action", &SYSTEM_PLUGIN_INFO.actions[action], NULL);
}

DeckPluginInfo *system_plugin_info(DeckPlugin *self) { return &SYSTEM_PLUGIN_INFO; }