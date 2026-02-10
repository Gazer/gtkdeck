#include "../../libobsws/libobsws.h"
#include "../deck_plugin.h"

void read_scene_name(JsonArray *array, guint index_, JsonNode *json, gpointer user_data) {
    GList **list = (GList **)user_data;

    JsonObject *object = json_node_get_object(json);

    if (json_object_has_member(object, "name")) {
        JsonNode *node = json_object_get_member(object, "name");
        if (JSON_NODE_HOLDS_VALUE(node)) {
            char *name = json_node_get_string(node);
            printf("> %s\n", name);
            (*list) = g_list_append(*list, g_strdup(name));
        }
    }
}

void on_scene_list(JsonObject *json, gpointer user_data) {
    char *current_scene = NULL;
    GList *scenes = NULL;

    // Verificar que user_data sea válido
    if (user_data == NULL || !GTK_IS_LIST_STORE(user_data)) {
        printf("ERROR: Invalid store in on_scene_list\n");
        return;
    }

    printf("Store %p\n", user_data);
    GtkListStore *store = GTK_LIST_STORE(user_data);

    // OBS v5: los datos están en responseData
    JsonObject *response_data = json;
    if (json_object_has_member(json, "responseData")) {
        JsonNode *response_node = json_object_get_member(json, "responseData");
        if (JSON_NODE_HOLDS_OBJECT(response_node)) {
            response_data = json_node_get_object(response_node);
        }
    }

    // OBS v5: current-scene -> currentProgramSceneName
    if (json_object_has_member(response_data, "currentProgramSceneName")) {
        JsonNode *node = json_object_get_member(response_data, "currentProgramSceneName");
        if (JSON_NODE_HOLDS_VALUE(node)) {
            current_scene = (char *)json_node_get_string(node);
            printf("Current scene: %s\n", current_scene ? current_scene : "(null)");
        }
    }

    // OBS v5: Las escenas tienen sceneName en lugar de name
    if (json_object_has_member(response_data, "scenes")) {
        JsonNode *node = json_object_get_member(response_data, "scenes");
        if (JSON_NODE_HOLDS_ARRAY(node)) {
            JsonArray *array = json_node_get_array(node);
            guint len = json_array_get_length(array);
            printf("Got %u scenes\n", len);

            GtkTreeIter iter;
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
                        }
                    }
                }
            }
        }
    }
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
    g_autofree gchar *current_scene;

    g_object_get(G_OBJECT(self), "scene", &current_scene, NULL);

    GtkTreeIter iter;
    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);

    printf(".. %p\n", store);
    ObsWs *ws = obs_ws_new();
    obs_ws_get_scenes(ws, on_scene_list, store);

    // gtk_list_store_append(store, &iter);
    // gtk_list_store_set(store, &iter, 0, "Previous Track", -1);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "height", 30, "width", 200, NULL);

    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(scene_changed), self);

    // gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current_key);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), GTK_CELL_RENDERER(renderer), "text", 0,
                                   NULL);

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