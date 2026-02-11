#include "../../libobsws/libobsws.h"
#include "../obs_action_types.h"
#include "../obs_plugin.h"

/* Private data for Change Scene action */
typedef struct {
    gchar *scene;
} ChangeSceneData;

/* Callback for scene changed event */
static void on_scene_changed(JsonObject *json, gpointer user_data) {
    if (user_data == NULL || !G_IS_OBJECT(user_data)) {
        return;
    }

    OBSPlugin *self = OBS_PLUGIN(user_data);
    ChangeSceneData *data = obs_plugin_get_action_data(self);
    if (data == NULL) {
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
           data->scene ? data->scene : "(null)");

    if (g_strcmp0(text, data->scene) == 0) {
        deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_SELECTED);
    } else {
        deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_NORMAL);
    }
}

/* Lifecycle methods */
static void change_scene_init(OBSPlugin *self) {
    ChangeSceneData *data = g_new0(ChangeSceneData, 1);
    data->scene = NULL;
    obs_plugin_set_action_data(self, data);

    ObsWs *ws = obs_ws_new();
    obs_ws_register_callback(ws, "CurrentProgramSceneChanged", on_scene_changed, self);
}

static void change_scene_destroy(OBSPlugin *self) {
    ChangeSceneData *data = obs_plugin_get_action_data(self);
    if (data != NULL) {
        if (data->scene != NULL) {
            g_free(data->scene);
        }
        g_free(data);
        obs_plugin_set_action_data(self, NULL);
    }
}

/* Config UI */
typedef struct {
    GtkComboBox *combo;
    gchar *selected_scene;
} SceneListData;

static void on_scene_list(JsonObject *json, gpointer user_data) {
    SceneListData *data = (SceneListData *)user_data;

    if (data == NULL || data->combo == NULL || !GTK_IS_COMBO_BOX(data->combo)) {
        if (data != NULL) {
            g_free(data->selected_scene);
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

    if (json_object_has_member(response_data, "scenes")) {
        JsonNode *node = json_object_get_member(response_data, "scenes");
        if (JSON_NODE_HOLDS_ARRAY(node)) {
            JsonArray *array = json_node_get_array(node);
            guint len = json_array_get_length(array);

            GtkTreeIter iter;
            gint active_index = -1;
            gint index = 0;
            for (guint i = 0; i < len; i++) {
                JsonNode *scene_node = json_array_get_element(array, i);
                if (JSON_NODE_HOLDS_OBJECT(scene_node)) {
                    JsonObject *scene_obj = json_node_get_object(scene_node);
                    if (json_object_has_member(scene_obj, "sceneName")) {
                        JsonNode *name_node = json_object_get_member(scene_obj, "sceneName");
                        if (JSON_NODE_HOLDS_VALUE(name_node)) {
                            const char *name = json_node_get_string(name_node);
                            gtk_list_store_append(store, &iter);
                            gtk_list_store_set(store, &iter, 0, name, -1);

                            if (data->selected_scene != NULL &&
                                g_strcmp0(name, data->selected_scene) == 0) {
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

    g_free(data->selected_scene);
    g_free(data);
}

static void scene_changed(GtkComboBox *widget, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    ChangeSceneData *data = obs_plugin_get_action_data(self);
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        char *scene;
        GtkTreeModel *store = gtk_combo_box_get_model(widget);
        gtk_tree_model_get(store, &iter, 0, &scene, -1);

        printf("Got %s\n", scene);

        if (data->scene != NULL) {
            g_free(data->scene);
        }
        data->scene = g_strdup(scene);
        g_free(scene);
    }
}

static void change_scene_config(OBSPlugin *self, GtkBox *parent) {
    ChangeSceneData *data = obs_plugin_get_action_data(self);

    GtkWidget *label = gtk_label_new("Scene");
    gchar *current_scene = g_strdup(data->scene);

    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "height", 30, "width", 200, NULL);

    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(scene_changed), self);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), GTK_CELL_RENDERER(renderer), "text", 0,
                                   NULL);

    SceneListData *list_data = g_new0(SceneListData, 1);
    list_data->combo = GTK_COMBO_BOX(combo);
    list_data->selected_scene = g_strdup(current_scene);

    ObsWs *ws = obs_ws_new();
    obs_ws_get_scenes(ws, on_scene_list, list_data);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(store));
    g_free(current_scene);
}

/* Execution */
static void change_scene_exec(OBSPlugin *self) {
    ChangeSceneData *data = obs_plugin_get_action_data(self);
    if (data != NULL && data->scene != NULL) {
        ObsWs *ws = obs_ws_new();
        obs_ws_set_current_scene(ws, data->scene);
    }
}

/* Serialization */
static void change_scene_save(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ChangeSceneData *data = obs_plugin_get_action_data(self);
    if (data != NULL && data->scene != NULL) {
        g_key_file_set_string(key_file, group, "scene", data->scene);
    }
}

static void change_scene_load(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ChangeSceneData *data = obs_plugin_get_action_data(self);
    g_autofree char *scene = g_key_file_get_string(key_file, group, "scene", NULL);

    if (scene != NULL) {
        if (data->scene != NULL) {
            g_free(data->scene);
        }
        data->scene = g_strdup(scene);
    }
}

/* VTable export */
const OBSActionVTable obs_change_scene_vtable = {
    .init = change_scene_init,
    .destroy = change_scene_destroy,
    .config = change_scene_config,
    .exec = change_scene_exec,
    .save = change_scene_save,
    .load = change_scene_load,
    .render = NULL, /* Use default render */
};
