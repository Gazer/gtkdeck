#include "obs_plugin.h"

#include "change_scene.h"
#include "../libobsws/libobsws.h"

DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action);
DeckPlugin *obs_plugin_clone_with_code(DeckPlugin *self, int code);
DeckPluginInfo *obs_plugin_info(DeckPlugin *self);

typedef struct _OBSPluginPrivate {
    gchar *scene;
} OBSPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(OBSPlugin, obs_plugin, DECK_TYPE_PLUGIN)

typedef enum { SCENE = 1, N_PROPERTIES } OBSPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

typedef enum { CHANGE_SCENE = 0, N_ACTIONS } OBSActionCodes;

DeckPluginInfo OBS_PLUGIN_INFO = {
    "OBS",
    N_ACTIONS,
    {
        {BUTTON_MODE_TOGGLE, "Change Scene", "/ar/com/p39/gtkdeck/plugins/obs-scene.png",
         "/ar/com/p39/gtkdeck/plugins/obs-scene-selected.png", CHANGE_SCENE, change_scene_config,
         change_scene_exec, change_scene_save, change_scene_load},
        // {BUTTON_MODE_NORMAL, "Text", "/ar/com/p39/gtkdeck/plugins/obs-text.png", TEXT,
        // text_config,
        //  text_exec, text_save, text_load},
        // {BUTTON_MODE_NORMAL, "Multimedia", "/ar/com/p39/gtkdeck/plugins/obs-previous.png",
        //  MULTIMEDIA, media_config, media_exec, media_save, media_load},
    },
};

static void obs_plugin_init(OBSPlugin *self) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    // Set default values
    priv->scene = NULL;
}

static void obs_plugin_finalize(GObject *object) {
    OBSPlugin *self = OBS_PLUGIN(object);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    // Free memory if needed
    if (priv->scene != NULL) {
        g_free(priv->scene);
    }

    G_OBJECT_CLASS(obs_plugin_parent_class)->finalize(object);
}

static void obs_plugin_set_property(GObject *object, guint property_id, const GValue *value,
                                    GParamSpec *pspec) {
    OBSPlugin *self = OBS_PLUGIN(object);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    switch ((OBSPluginProperty)property_id) {
    case SCENE: {
        if (priv->scene != NULL) {
            g_free(priv->scene);
        }
        priv->scene = g_value_dup_string(value);
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}
static void obs_plugin_get_property(GObject *object, guint property_id, GValue *value,
                                    GParamSpec *pspec) {
    OBSPlugin *self = OBS_PLUGIN(object);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    switch ((OBSPluginProperty)property_id) {
    case SCENE: {
        g_value_set_string(value, priv->scene);
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void obs_plugin_class_init(OBSPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    /* implement pure virtual function. */
    deck_plugin_klass->info = obs_plugin_info;
    deck_plugin_klass->clone = obs_plugin_clone;
    deck_plugin_klass->clone_with_code = obs_plugin_clone_with_code;
    // deck_plugin_klass->save = obs_plugin_save;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = obs_plugin_set_property;
    object_class->get_property = obs_plugin_get_property;

    object_class->finalize = obs_plugin_finalize;

    obj_properties[SCENE] =
        g_param_spec_string("scene", "scene", "scene.", NULL, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

DeckPlugin *obs_plugin_new() { return g_object_new(OBS_TYPE_PLUGIN, "name", "OBSPlugin", NULL); }

static void on_scene_changed(JsonObject *json, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);
    gchar *text = json_object_get_string_value(json, "scene-name");

    if (g_strcmp0(text, priv->scene) == 0) {
        deck_plugin_set_state(self, BUTTON_STATE_SELECTED);
    } else {
        deck_plugin_set_state(self, BUTTON_STATE_NORMAL);
    }
}

DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action) {
    ObsWs *ws = obs_plugin_new();
    DeckPlugin *clone = g_object_new(OBS_TYPE_PLUGIN, "name", "OBSPlugin", "action",
                                     &OBS_PLUGIN_INFO.actions[action], NULL);

    obs_ws_register_callback("SwitchScenes", on_scene_changed, clone);
    return clone;
}

DeckPluginInfo *obs_plugin_info(DeckPlugin *self) { return &OBS_PLUGIN_INFO; }

DeckPlugin *obs_plugin_clone_with_code(DeckPlugin *self, int code) {
    for (int i = 0; i < N_ACTIONS; i++) {
        if (code == OBS_PLUGIN_INFO.actions[i].code) {
            printf("  FOUND ACTION %i\n", i);
            return obs_plugin_clone(self, i);
        }
    }

    return NULL;
}
