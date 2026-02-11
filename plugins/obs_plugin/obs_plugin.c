#include "obs_plugin.h"
#include "obs_action_types.h"

#include "../../libobsws/libobsws.h"

/* Action implementations */
extern const OBSActionVTable obs_change_scene_vtable;
extern const OBSActionVTable obs_mute_source_vtable;
extern const OBSActionVTable obs_toggle_recording_vtable;
extern const OBSActionVTable obs_toggle_source_visibility_vtable;
extern const OBSActionVTable obs_chapter_marker_vtable;
extern const OBSActionVTable obs_screenshot_vtable;

/* Action codes */
typedef enum {
    CHANGE_SCENE = 0,
    MUTE_SOURCE = 1,
    TOGGLE_RECORDING = 2,
    TOGGLE_SOURCE_VISIBILITY = 3,
    CHAPTER_MARKER = 4,
    SCREENSHOT = 5,
    N_ACTIONS
} OBSActionCodes;

/* Forward declarations */
static DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action);
static DeckPlugin *obs_plugin_clone_with_code(DeckPlugin *self, int code);
static DeckPluginInfo *obs_plugin_info(DeckPlugin *self);
static void obs_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height);

static void obs_plugin_config_widget(DeckPlugin *self, GtkBox *parent);
static void obs_plugin_exec(DeckPlugin *self);

/* Private data structure */
typedef struct _OBSPluginPrivate {
    gboolean obs_connected;
    const OBSActionVTable *vtable;
    void *action_data;
} OBSPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(OBSPlugin, obs_plugin, DECK_TYPE_PLUGIN)

typedef enum { OBS_CONNECTED = 1, N_PROPERTIES } OBSPluginProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

/* Helper functions for accessing private data */
void obs_plugin_set_action_data(OBSPlugin *self, void *data) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);
    priv->action_data = data;
}

void *obs_plugin_get_action_data(OBSPlugin *self) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);
    return priv->action_data;
}

const OBSActionVTable *obs_plugin_get_vtable(OBSPlugin *self) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);
    return priv->vtable;
}

/* Property accessors */
static void obs_plugin_set_property(GObject *object, guint property_id, const GValue *value,
                                    GParamSpec *pspec) {
    OBSPlugin *self = OBS_PLUGIN(object);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    switch ((OBSPluginProperty)property_id) {
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
    case OBS_CONNECTED: {
        g_value_set_boolean(value, priv->obs_connected);
        break;
    }
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

/* Instance init */
static void obs_plugin_init(OBSPlugin *self) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    priv->obs_connected = FALSE;
    priv->vtable = NULL;
    priv->action_data = NULL;
}

/* Finalize */
static void obs_plugin_finalize(GObject *object) {
    OBSPlugin *self = OBS_PLUGIN(object);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(self);

    /* Call action-specific destroy if set */
    if (priv->vtable != NULL && priv->vtable->destroy != NULL) {
        priv->vtable->destroy(self);
    }

    G_OBJECT_CLASS(obs_plugin_parent_class)->finalize(object);
}

/* Class init */
static void obs_plugin_class_init(OBSPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    /* Virtual functions */
    deck_plugin_klass->info = obs_plugin_info;
    deck_plugin_klass->clone = obs_plugin_clone;
    deck_plugin_klass->clone_with_code = obs_plugin_clone_with_code;
    deck_plugin_klass->render = obs_plugin_render;
    deck_plugin_klass->config_widget = obs_plugin_config_widget;
    deck_plugin_klass->exec = obs_plugin_exec;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->set_property = obs_plugin_set_property;
    object_class->get_property = obs_plugin_get_property;
    object_class->finalize = obs_plugin_finalize;

    obj_properties[OBS_CONNECTED] = g_param_spec_boolean(
        "obs_connected", "obs_connected", "obs_connected.", FALSE, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

/* Render - delegates to vtable render if available, otherwise default */
static void obs_plugin_render(DeckPlugin *self, cairo_t *cr, int width, int height) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));

    if (priv->vtable != NULL && priv->vtable->render != NULL) {
        /* Action-specific render when connected */
        priv->vtable->render(OBS_PLUGIN(self), cr, width, height);
    }

    if (priv->obs_connected == FALSE) {
        /* Default render for disconnected state */
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

/* Config widget - delegates to vtable */
static void obs_plugin_config_widget(DeckPlugin *self, GtkBox *parent) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));
    if (priv->vtable != NULL && priv->vtable->config != NULL) {
        priv->vtable->config(OBS_PLUGIN(self), parent);
    }
}

/* Execute - delegates to vtable */
static void obs_plugin_exec(DeckPlugin *self) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));
    if (priv->vtable != NULL && priv->vtable->exec != NULL) {
        priv->vtable->exec(OBS_PLUGIN(self));
    }
}

static void action_config_wrapper(DeckPlugin *self, GtkBox *parent) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));
    if (priv->vtable != NULL && priv->vtable->config != NULL) {
        priv->vtable->config(OBS_PLUGIN(self), parent);
    }
}

static void action_exec_wrapper(DeckPlugin *self) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));
    if (priv->vtable != NULL && priv->vtable->exec != NULL) {
        priv->vtable->exec(OBS_PLUGIN(self));
    }
}

static void action_save_wrapper(DeckPlugin *self, char *group, GKeyFile *key_file) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));
    if (priv->vtable != NULL && priv->vtable->save != NULL) {
        priv->vtable->save(OBS_PLUGIN(self), group, key_file);
    }
}

static void action_load_wrapper(DeckPlugin *self, char *group, GKeyFile *key_file) {
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(OBS_PLUGIN(self));
    if (priv->vtable != NULL && priv->vtable->load != NULL) {
        priv->vtable->load(OBS_PLUGIN(self), group, key_file);
    }
}

DeckPluginInfo OBS_PLUGIN_INFO = {
    "OBS",
    N_ACTIONS,
    {
        {BUTTON_MODE_TOGGLE, "Change Scene", "/ar/com/p39/gtkdeck/plugins/obs-scene.png",
         "/ar/com/p39/gtkdeck/plugins/obs-scene-selected.png", CHANGE_SCENE, action_config_wrapper,
         action_exec_wrapper, action_save_wrapper, action_load_wrapper},
        {BUTTON_MODE_TOGGLE, "Mute Source", "/ar/com/p39/gtkdeck/plugins/obs-mute.png",
         "/ar/com/p39/gtkdeck/plugins/obs-mute-selected.png", MUTE_SOURCE, action_config_wrapper,
         action_exec_wrapper, action_save_wrapper, action_load_wrapper},
        {BUTTON_MODE_TOGGLE, "Toggle Recording", "/ar/com/p39/gtkdeck/plugins/obs-recording.png",
         "/ar/com/p39/gtkdeck/plugins/obs-recording-selected.png", TOGGLE_RECORDING,
         action_config_wrapper, action_exec_wrapper, action_save_wrapper, action_load_wrapper},
        {BUTTON_MODE_TOGGLE, "Toggle Source Visibility",
         "/ar/com/p39/gtkdeck/plugins/obs-source.png",
         "/ar/com/p39/gtkdeck/plugins/obs-source-selected.png", TOGGLE_SOURCE_VISIBILITY,
         action_config_wrapper, action_exec_wrapper, action_save_wrapper, action_load_wrapper},
        {BUTTON_MODE_NORMAL, "Chapter Marker", "/ar/com/p39/gtkdeck/plugins/obs-chapter.png",
         "/ar/com/p39/gtkdeck/plugins/obs-chapter-selected.png", CHAPTER_MARKER,
         action_config_wrapper, action_exec_wrapper, action_save_wrapper, action_load_wrapper},
        {BUTTON_MODE_NORMAL, "Screenshot", "/ar/com/p39/gtkdeck/plugins/obs-screenshot.png",
         "/ar/com/p39/gtkdeck/plugins/obs-screenshot-selected.png", SCREENSHOT,
         action_config_wrapper, action_exec_wrapper, action_save_wrapper, action_load_wrapper},
    },
};

/* WebSocket connection callback */
static void on_ws_connected(JsonObject *json, gpointer user_data) {
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

    g_object_set(plugin, "obs_connected", connected, NULL);
    deck_plugin_reset(plugin);

    if (connected) {
        ObsWs *ws = obs_ws_new();
        obs_ws_get_current_scene(ws, NULL, NULL);
    }
}

/* Clone factory - assigns vtable based on action */
static DeckPlugin *obs_plugin_clone(DeckPlugin *self, int action) {
    OBSPlugin *clone = g_object_new(OBS_TYPE_PLUGIN, "name", "OBSPlugin", "action",
                                    &OBS_PLUGIN_INFO.actions[action], NULL);
    OBSPluginPrivate *priv = obs_plugin_get_instance_private(clone);

    /* Assign vtable based on action code */
    switch (action) {
    case CHANGE_SCENE:
        priv->vtable = &obs_change_scene_vtable;
        break;
    case MUTE_SOURCE:
        priv->vtable = &obs_mute_source_vtable;
        break;
    case TOGGLE_RECORDING:
        priv->vtable = &obs_toggle_recording_vtable;
        break;
    case TOGGLE_SOURCE_VISIBILITY:
        priv->vtable = &obs_toggle_source_visibility_vtable;
        break;
    case CHAPTER_MARKER:
        priv->vtable = &obs_chapter_marker_vtable;
        break;
    case SCREENSHOT:
        priv->vtable = &obs_screenshot_vtable;
        break;
    default:
        priv->vtable = NULL;
        break;
    }

    /* Initialize action-specific data */
    if (priv->vtable != NULL && priv->vtable->init != NULL) {
        priv->vtable->init(clone);
    }

    /* Register global connection callback */
    ObsWs *ws = obs_ws_new();
    obs_ws_register_callback(ws, "Identified", on_ws_connected, DECK_PLUGIN(clone));

    return DECK_PLUGIN(clone);
}

/* Clone by action code */
static DeckPlugin *obs_plugin_clone_with_code(DeckPlugin *self, int code) {
    for (int i = 0; i < N_ACTIONS; i++) {
        if (code == OBS_PLUGIN_INFO.actions[i].code) {
            return obs_plugin_clone(self, i);
        }
    }

    return NULL;
}

/* Plugin info */
static DeckPluginInfo *obs_plugin_info(DeckPlugin *self) { return &OBS_PLUGIN_INFO; }

/* Public constructor */
DeckPlugin *obs_plugin_new() {
    obs_ws_new();
    return g_object_new(OBS_TYPE_PLUGIN, "name", "OBSPlugin", NULL);
}
