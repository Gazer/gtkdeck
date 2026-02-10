#include "obs_plugin.h"

#include "../libobsws/libobsws.h"
#include "change_scene.h"
#include "mute_source.h"
#include "toggle_recording.h"

static DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action);
static DeckPlugin *obs_plugin_clone_with_code(DeckPlugin *self, int code);
static DeckPluginInfo *obs_plugin_info(DeckPlugin *self);
static void obs_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height);

typedef struct _OBSPluginPrivate {
    gchar *scene;
    gchar *source;
    gboolean obs_connected;
} OBSPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(OBSPlugin, obs_plugin, DECK_TYPE_PLUGIN)

typedef enum { SCENE = 1, SOURCE, OBS_CONNECTED, N_PROPERTIES } OBSPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

typedef enum { CHANGE_SCENE = 0, MUTE_SOURCE = 1, TOGGLE_RECORDING = 2, N_ACTIONS } OBSActionCodes;

DeckPluginInfo OBS_PLUGIN_INFO = {
    "OBS",
    N_ACTIONS,
    {
        {BUTTON_MODE_TOGGLE, "Change Scene", "/ar/com/p39/gtkdeck/plugins/obs-scene.png",
         "/ar/com/p39/gtkdeck/plugins/obs-scene-selected.png", CHANGE_SCENE, change_scene_config,
         change_scene_exec, change_scene_save, change_scene_load},
        {BUTTON_MODE_TOGGLE, "Mute Source", "/ar/com/p39/gtkdeck/plugins/obs-mute.png",
         "/ar/com/p39/gtkdeck/plugins/obs-mute-selected.png", MUTE_SOURCE, mute_source_config,
         mute_source_exec, mute_source_save, mute_source_load},
        {BUTTON_MODE_TOGGLE, "Toggle Recording", "/ar/com/p39/gtkdeck/plugins/obs-recording.png",
         "/ar/com/p39/gtkdeck/plugins/obs-recording-selected.png", TOGGLE_RECORDING,
         toggle_recording_config, toggle_recording_exec, toggle_recording_save,
         toggle_recording_load},
    },
};

static void obs_plugin_init(OBSPlugin *self) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    priv->scene = NULL;
    priv->source = NULL;
    priv->obs_connected = FALSE;
}

static void obs_plugin_finalize(GObject *object) {
    OBSPlugin *self = OBS_PLUGIN(object);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    if (priv->scene != NULL) {
        g_free(priv->scene);
    }

    if (priv->source != NULL) {
        g_free(priv->source);
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
    case SOURCE: {
        if (priv->source != NULL) {
            g_free(priv->source);
        }
        priv->source = g_value_dup_string(value);
        break;
    }
    case OBS_CONNECTED: {
        priv->obs_connected = g_value_get_boolean(value);
        break;
    }
    default:
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
    case SOURCE: {
        g_value_set_string(value, priv->source);
        break;
    }
    case OBS_CONNECTED: {
        g_value_set_boolean(value, priv->obs_connected);
        break;
    }
    default:
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

    obj_properties[SOURCE] =
        g_param_spec_string("source", "source", "source.", NULL, G_PARAM_READWRITE);

    obj_properties[OBS_CONNECTED] = g_param_spec_boolean(
        "obs_connected", "obs_connected", "obs_connected.", FALSE, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

static void obs_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));

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

DeckPlugin *obs_plugin_new() {
    obs_ws_new();
    return g_object_new(OBS_TYPE_PLUGIN, "name", "OBSPlugin", NULL);
}

static void on_scene_changed(JsonObject *json, gpointer user_data) {
    if (user_data == NULL || !G_IS_OBJECT(user_data)) {
        return;
    }

    OBSPlugin *self = OBS_PLUGIN(user_data);
    if (self == NULL) {
        return;
    }

    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);
    if (priv == NULL) {
        return;
    }

    gchar *text = NULL;
    if (json_object_has_member(json, "eventData")) {
        JsonNode *event_node = json_object_get_member(json, "eventData");
        if (JSON_NODE_HOLDS_OBJECT(event_node)) {
            JsonObject *event_data = json_node_get_object(event_node);
            text = (gchar *)json_object_get_string_value(event_data, "sceneName");
        }
    }

    if (text == NULL) {
        text = (gchar *)json_object_get_string_value(json, "sceneName");
    }

    printf("Scene changed to: %s (our scene: %s)\n", text ? text : "(null)",
           priv->scene ? priv->scene : "(null)");

    if (g_strcmp0(text, priv->scene) == 0) {
        deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_SELECTED);
    } else {
        deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_NORMAL);
    }
}

static void on_ws_connected(JsonObject *json, gpointer user_data) {
    printf("enter: on_ws_connected\n");
    if (user_data == NULL || !G_IS_OBJECT(user_data)) {
        return;
    }

    DeckPlugin *plugin = DECK_PLUGIN(user_data);
    if (plugin == NULL) {
        return;
    }

    gboolean connected = TRUE;

    if (json_object_has_member(json, "eventData")) {
        JsonNode *event_node = json_object_get_member(json, "eventData");
        if (JSON_NODE_HOLDS_OBJECT(event_node)) {
            JsonObject *event_data = json_node_get_object(event_node);
            connected = json_object_get_boolean_value(event_data, "connected");
        }
    } else {
        connected = json_object_get_boolean_value(json, "connected");
    }

    printf("Connected callback: %d\n", connected);
    g_object_set(plugin, "obs_connected", connected, NULL);
    deck_plugin_reset(plugin);

    if (connected) {
        ObsWs *ws = obs_ws_new();
        obs_ws_get_current_scene(ws, NULL, NULL);
    }
}

static void on_input_mute_changed(JsonObject *json, gpointer user_data) {
    if (user_data == NULL || !G_IS_OBJECT(user_data)) {
        return;
    }

    OBSPlugin *self = OBS_PLUGIN(user_data);
    if (self == NULL) {
        return;
    }

    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);
    if (priv == NULL) {
        return;
    }

    gchar *input_name = NULL;
    gboolean input_muted = FALSE;

    if (json_object_has_member(json, "eventData")) {
        JsonNode *event_node = json_object_get_member(json, "eventData");
        if (JSON_NODE_HOLDS_OBJECT(event_node)) {
            JsonObject *event_data = json_node_get_object(event_node);
            input_name = (gchar *)json_object_get_string_value(event_data, "inputName");
            input_muted = json_object_get_boolean_value(event_data, "inputMuted");
        }
    }

    if (input_name == NULL) {
        return;
    }

    if (g_strcmp0(input_name, priv->source) == 0) {
        if (input_muted) {
            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_SELECTED);
        } else {
            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_NORMAL);
        }
    }
}

static void on_record_state_changed(JsonObject *json, gpointer user_data) {
    if (user_data == NULL || !G_IS_OBJECT(user_data)) {
        return;
    }

    DeckPlugin *self = DECK_PLUGIN(user_data);
    if (self == NULL) {
        return;
    }

    const gchar *state = NULL;

    if (json_object_has_member(json, "eventData")) {
        JsonNode *event_node = json_object_get_member(json, "eventData");
        if (JSON_NODE_HOLDS_OBJECT(event_node)) {
            JsonObject *event_data = json_node_get_object(event_node);
            state = json_object_get_string_value(event_data, "outputState");
        }
    }

    if (state == NULL) {
        return;
    }

    if (g_strcmp0(state, "OBS_WEBSOCKET_OUTPUT_STARTED") == 0) {
        deck_plugin_set_state(self, BUTTON_STATE_SELECTED);
    } else if (g_strcmp0(state, "OBS_WEBSOCKET_OUTPUT_STOPPED") == 0) {
        deck_plugin_set_state(self, BUTTON_STATE_NORMAL);
    }
}

DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action) {
    DeckPlugin *clone = g_object_new(OBS_TYPE_PLUGIN, "name", "OBSPlugin", "action",
                                     &OBS_PLUGIN_INFO.actions[action], NULL);

    ObsWs *ws = obs_ws_new();

    if (action == CHANGE_SCENE) {
        obs_ws_register_callback(ws, "CurrentProgramSceneChanged", on_scene_changed, clone);
    } else if (action == MUTE_SOURCE) {
        obs_ws_register_callback(ws, "InputMuteStateChanged", on_input_mute_changed, clone);
    } else if (action == TOGGLE_RECORDING) {
        obs_ws_register_callback(ws, "RecordStateChanged", on_record_state_changed, clone);
    }

    obs_ws_register_callback(ws, "Identified", on_ws_connected, clone);

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
