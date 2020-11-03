#include "obs_plugin.h"

#include "change_scene.h"
#include "../libobsws/libobsws.h"

static DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action);
static DeckPlugin *obs_plugin_clone_with_code(DeckPlugin *self, int code);
static DeckPluginInfo *obs_plugin_info(DeckPlugin *self);
static void obs_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height);

typedef struct _OBSPluginPrivate {
    gchar *scene;
    gboolean obs_connected;
} OBSPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(OBSPlugin, obs_plugin, DECK_TYPE_PLUGIN)

typedef enum { SCENE = 1, OBS_CONNECTED, N_PROPERTIES } OBSPluginProperty;

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
    priv->obs_connected = NULL;
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
    case OBS_CONNECTED: {
        priv->obs_connected = g_value_get_boolean(value);
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
    case OBS_CONNECTED: {
        g_value_set_boolean(value, priv->obs_connected);
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
    deck_plugin_klass->render = obs_plugin_render;

    // deck_plugin_klass->save = obs_plugin_save;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = obs_plugin_set_property;
    object_class->get_property = obs_plugin_get_property;

    object_class->finalize = obs_plugin_finalize;

    obj_properties[SCENE] =
        g_param_spec_string("scene", "scene", "scene.", NULL, G_PARAM_READWRITE);

    obj_properties[OBS_CONNECTED] = g_param_spec_boolean(
        "obs_connected", "obs_connected", "obs_connected.", FALSE, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static obs_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    if (priv->obs_connected == FALSE) {
        cairo_save(cr);

        int margin = 50;

        cairo_set_line_width(cr, 5);

        cairo_move_to(cr, width / 2.0, margin);
        cairo_line_to(cr, width - margin, height - margin);
        cairo_line_to(cr, margin, height - margin);
        cairo_close_path(cr);
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_fill(cr);

        cairo_set_line_width(cr, 10);
        cairo_move_to(cr, width / 2, margin * 2);
        cairo_line_to(cr, width / 2, height - margin * 2.5);
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_stroke(cr);

        cairo_set_line_width(cr, 10);
        cairo_move_to(cr, width / 2, height - margin * 2);
        cairo_line_to(cr, width / 2, height - margin * 1.8);
        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0);
        cairo_stroke(cr);

        cairo_restore(cr);
    }
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

static void on_ws_connected(JsonObject *json, gpointer user_data) {
    DeckPlugin *plugin = DECK_PLUGIN(user_data);

    gboolean connected = json_object_get_boolean_value(json, "value");

    printf("Connected %d\n", connected);
    g_object_set(plugin, "obs_connected", connected, NULL);
    deck_plugin_reset(plugin);

    if (connected) {
        ObsWs *ws = obs_plugin_new();
        obs_ws_get_current_scene(ws, NULL, NULL);
    }
}

DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action) {
    ObsWs *ws = obs_plugin_new();
    DeckPlugin *clone = g_object_new(OBS_TYPE_PLUGIN, "name", "OBSPlugin", "action",
                                     &OBS_PLUGIN_INFO.actions[action], NULL);

    obs_ws_register_callback("SwitchScenes", on_scene_changed, clone);
    obs_ws_register_callback("Connected", on_ws_connected, clone);

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
