#include "../../libobsws/libobsws.h"
#include "../obs_action_types.h"
#include "../obs_plugin.h"

/* Private data for Toggle Recording action */
/* No data needed - state is tracked via OBS events */

/* Callback for record state changed event */
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

/* Lifecycle methods */
static void toggle_recording_init(OBSPlugin *self) {
    /* No action_data needed */
    obs_plugin_set_action_data(self, NULL);

    ObsWs *ws = obs_ws_new();
    obs_ws_register_callback(ws, "RecordStateChanged", on_record_state_changed, self);
}

static void toggle_recording_destroy(OBSPlugin *self) {
    /* Nothing to clean up */
    obs_plugin_set_action_data(self, NULL);
}

/* Config UI */
static void toggle_recording_config(OBSPlugin *self, GtkBox *parent) {
    GtkWidget *label = gtk_label_new("Toggle OBS Recording");
    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
}

/* Execution */
static void toggle_recording_exec(OBSPlugin *self) {
    ObsWs *ws = obs_ws_new();
    obs_ws_toggle_record(ws);
}

/* Serialization */
static void toggle_recording_save(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    /* Nothing to save */
}

static void toggle_recording_load(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    /* Nothing to load */
}

/* VTable export */
const OBSActionVTable obs_toggle_recording_vtable = {
    .init = toggle_recording_init,
    .destroy = toggle_recording_destroy,
    .config = toggle_recording_config,
    .exec = toggle_recording_exec,
    .save = toggle_recording_save,
    .load = toggle_recording_load,
    .render = NULL, /* Use default render */
};
