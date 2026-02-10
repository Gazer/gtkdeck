#include "../../libobsws/libobsws.h"
#include "../deck_plugin.h"

typedef struct {
    GtkComboBox *combo;
    gchar *selected_input;
} InputListData;

void on_input_list(JsonObject *json, gpointer user_data) {
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
    DeckPlugin *self = DECK_PLUGIN(user_data);
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        char *input;
        GtkTreeModel *store = gtk_combo_box_get_model(widget);
        gtk_tree_model_get(store, &iter, 0, &input, -1);
        g_object_set(G_OBJECT(self), "source", input, NULL);
        g_free(input);
    }
}

void mute_source_config(DeckPlugin *self, GtkBox *parent) {
    GtkWidget *label = gtk_label_new("Audio Source");
    g_autofree gchar *current_source = NULL;

    g_object_get(G_OBJECT(self), "source", &current_source, NULL);

    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "height", 30, "width", 200, NULL);

    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(input_changed), self);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), GTK_CELL_RENDERER(renderer), "text", 0,
                                   NULL);

    InputListData *data = g_new0(InputListData, 1);
    data->combo = GTK_COMBO_BOX(combo);
    data->selected_input = g_strdup(current_source);

    ObsWs *ws = obs_ws_new();
    obs_ws_get_input_list(ws, on_input_list, data);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(store));
}

void mute_source_exec(DeckPlugin *self) {
    char *source;
    g_object_get(G_OBJECT(self), "source", &source, NULL);

    if (source != NULL) {
        ObsWs *ws = obs_ws_new();
        obs_ws_toggle_input_mute(ws, source);
        g_free(source);
    }
}

void mute_source_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    gchar *source;
    g_object_get(G_OBJECT(self), "source", &source, NULL);

    if (source != NULL) {
        g_key_file_set_string(key_file, group, "source", source);
        g_free(source);
    }
}

void mute_source_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    g_autofree char *source = g_key_file_get_string(key_file, group, "source", NULL);

    if (source != NULL) {
        g_object_set(G_OBJECT(self), "source", source, NULL);
    }
}
