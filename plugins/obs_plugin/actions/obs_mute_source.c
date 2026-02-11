#include "../../libobsws/libobsws.h"
#include "../obs_action_types.h"
#include "../obs_plugin.h"

/* Private data for Mute Source action */
typedef struct {
    gchar *source;
} MuteSourceData;

/* Callback for input mute changed event */
static void on_input_mute_changed(JsonObject *json, gpointer user_data) {
    if (user_data == NULL || !G_IS_OBJECT(user_data)) {
        return;
    }

    OBSPlugin *self = OBS_PLUGIN(user_data);
    MuteSourceData *data = obs_plugin_get_action_data(self);
    if (data == NULL) {
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

    if (g_strcmp0(input_name, data->source) == 0) {
        if (input_muted) {
            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_SELECTED);
        } else {
            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_NORMAL);
        }
    }
}

/* Lifecycle methods */
static void mute_source_init(OBSPlugin *self) {
    MuteSourceData *data = g_new0(MuteSourceData, 1);
    data->source = NULL;
    obs_plugin_set_action_data(self, data);

    ObsWs *ws = obs_ws_new();
    obs_ws_register_callback(ws, "InputMuteStateChanged", on_input_mute_changed, self);
}

static void mute_source_destroy(OBSPlugin *self) {
    MuteSourceData *data = obs_plugin_get_action_data(self);
    if (data != NULL) {
        if (data->source != NULL) {
            g_free(data->source);
        }
        g_free(data);
        obs_plugin_set_action_data(self, NULL);
    }
}

/* Config UI */
typedef struct {
    GtkComboBox *combo;
    gchar *selected_input;
} InputListData;

static void on_input_list(JsonObject *json, gpointer user_data) {
    InputListData *data = (InputListData *)user_data;

    if (data == NULL || data->combo == NULL || !GTK_IS_COMBO_BOX(data->combo)) {
        if (data != NULL) {
            g_free(data->selected_input);
            g_free(data);
        }
        return;
    }

    GtkTreeModel *model = gtk_combo_box_get_model(data->combo);
    GtkListStore *store = GTK_LIST_STORE(model);

    JsonObject *response_data = json;
    if (json_object_has_member(json, "responseData")) {
        JsonNode *response_node = json_object_get_member(json, "responseData");
        if (JSON_NODE_HOLDS_OBJECT(response_node)) {
            response_data = json_node_get_object(response_node);
        }
    }

    if (json_object_has_member(response_data, "inputs")) {
        JsonNode *node = json_object_get_member(response_data, "inputs");
        if (JSON_NODE_HOLDS_ARRAY(node)) {
            JsonArray *array = json_node_get_array(node);
            guint len = json_array_get_length(array);

            GtkTreeIter iter;
            gint active_index = -1;
            gint index = 0;
            for (guint i = 0; i < len; i++) {
                JsonNode *input_node = json_array_get_element(array, i);
                if (JSON_NODE_HOLDS_OBJECT(input_node)) {
                    JsonObject *input_obj = json_node_get_object(input_node);
                    const char *kind = json_object_get_string_value(input_obj, "inputKind");
                    if (kind == NULL) {
                        continue;
                    }

                    gboolean is_audio =
                        g_str_has_prefix(kind, "pulse") || g_str_has_prefix(kind, "alsa") ||
                        g_str_has_prefix(kind, "jack") || g_str_has_prefix(kind, "pipewire") ||
                        g_str_has_prefix(kind, "wasapi") || g_str_has_prefix(kind, "coreaudio") ||
                        g_strcmp0(kind, "ffmpeg_source") == 0 || g_strcmp0(kind, "vlc_source") == 0;

                    if (!is_audio) {
                        continue;
                    }

                    if (json_object_has_member(input_obj, "inputName")) {
                        JsonNode *name_node = json_object_get_member(input_obj, "inputName");
                        if (JSON_NODE_HOLDS_VALUE(name_node)) {
                            const char *name = json_node_get_string(name_node);
                            gtk_list_store_append(store, &iter);
                            gtk_list_store_set(store, &iter, 0, name, -1);

                            if (data->selected_input != NULL &&
                                g_strcmp0(name, data->selected_input) == 0) {
                                active_index = index;
                            }
                            index++;
                        }
                    }
                }
            }

            if (active_index >= 0) {
                gtk_combo_box_set_active(data->combo, active_index);
            }
        }
    }

    g_free(data->selected_input);
    g_free(data);
}

static void input_changed(GtkComboBox *widget, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    MuteSourceData *data = obs_plugin_get_action_data(self);
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        char *input;
        GtkTreeModel *store = gtk_combo_box_get_model(widget);
        gtk_tree_model_get(store, &iter, 0, &input, -1);

        if (data->source != NULL) {
            g_free(data->source);
        }
        data->source = g_strdup(input);
        g_free(input);
    }
}

static void mute_source_config(OBSPlugin *self, GtkBox *parent) {
    MuteSourceData *data = obs_plugin_get_action_data(self);

    GtkWidget *label = gtk_label_new("Audio Source");
    gchar *current_source = g_strdup(data->source);

    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "height", 30, "width", 200, NULL);

    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(input_changed), self);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), GTK_CELL_RENDERER(renderer), "text", 0,
                                   NULL);

    InputListData *list_data = g_new0(InputListData, 1);
    list_data->combo = GTK_COMBO_BOX(combo);
    list_data->selected_input = g_strdup(current_source);

    ObsWs *ws = obs_ws_new();
    obs_ws_get_input_list(ws, on_input_list, list_data);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(store));
    g_free(current_source);
}

/* Execution */
static void mute_source_exec(OBSPlugin *self) {
    MuteSourceData *data = obs_plugin_get_action_data(self);
    if (data != NULL && data->source != NULL) {
        ObsWs *ws = obs_ws_new();
        obs_ws_toggle_input_mute(ws, data->source);
    }
}

/* Serialization */
static void mute_source_save(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    MuteSourceData *data = obs_plugin_get_action_data(self);
    if (data != NULL && data->source != NULL) {
        g_key_file_set_string(key_file, group, "source", data->source);
    }
}

static void mute_source_load(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    MuteSourceData *data = obs_plugin_get_action_data(self);
    g_autofree char *source = g_key_file_get_string(key_file, group, "source", NULL);

    if (source != NULL) {
        if (data->source != NULL) {
            g_free(data->source);
        }
        data->source = g_strdup(source);
    }
}

/* VTable export */
const OBSActionVTable obs_mute_source_vtable = {
    .init = mute_source_init,
    .destroy = mute_source_destroy,
    .config = mute_source_config,
    .exec = mute_source_exec,
    .save = mute_source_save,
    .load = mute_source_load,
    .render = NULL, /* Use default render */
};
