#include "../../libobsws/libobsws.h"
#include "../deck_plugin.h"

typedef struct {
    GtkComboBox *combo;
    gchar *selected_scene;
} SceneListData;

void on_scene_list(JsonObject *json, gpointer user_data) {
    SceneListData *data = (SceneListData *)user_data;

    if (data == NULL || data->combo == NULL || !GTK_IS_COMBO_BOX(data->combo)) {
        printf("ERROR: Invalid data in on_scene_list\n");
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
            printf("Got %u scenes\n", len);

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
                            printf("Scene: %s\n", name);
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
    DeckPlugin *self = DECK_PLUGIN(user_data);
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        char *scene;
        GtkTreeModel *store = gtk_combo_box_get_model(widget);
        gtk_tree_model_get(store, &iter, 0, &scene, -1);

        printf("Got %s\n", scene);

        g_object_set(G_OBJECT(self), "scene", scene, NULL);
        g_free(scene);
    }
}

void change_scene_config(DeckPlugin *self, GtkBox *parent) {
    GtkWidget *label = gtk_label_new("Scene");
    g_autofree gchar *current_scene = NULL;

    g_object_get(G_OBJECT(self), "scene", &current_scene, NULL);

    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "height", 30, "width", 200, NULL);

    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(scene_changed), self);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), GTK_CELL_RENDERER(renderer), "text", 0,
                                   NULL);

    SceneListData *data = g_new0(SceneListData, 1);
    data->combo = GTK_COMBO_BOX(combo);
    data->selected_scene = g_strdup(current_scene);

    ObsWs *ws = obs_ws_new();
    obs_ws_get_scenes(ws, on_scene_list, data);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(store));
}

void change_scene_exec(DeckPlugin *self) {
    char *scene;
    g_object_get(G_OBJECT(self), "scene", &scene, NULL);

    ObsWs *ws = obs_ws_new();
    obs_ws_set_current_scene(ws, scene);
}

void change_scene_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    gchar *scene;
    g_object_get(G_OBJECT(self), "scene", &scene, NULL);

    if (scene != NULL) {
        g_key_file_set_string(key_file, group, "scene", scene);
        g_free(scene);
    }
}

void change_scene_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    g_autofree char *scene = g_key_file_get_string(key_file, group, "scene", NULL);

    if (scene != NULL) {
        g_object_set(G_OBJECT(self), "scene", scene, NULL);
    }
}
