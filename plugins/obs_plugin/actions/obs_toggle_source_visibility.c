#include "../../libobsws/libobsws.h"
#include "../obs_action_types.h"
#include "../obs_plugin.h"

/* Private data for Toggle Source Visibility action */
typedef struct {
    gchar *scene_name;
    gchar *source_name;
    int scene_item_id;
    gboolean is_visible;
} ToggleSourceVisibilityData;

/* Callback for scene item enable state changed event */
static void on_scene_item_enable_changed(JsonObject *json, gpointer user_data) {
    if (user_data == NULL || !G_IS_OBJECT(user_data)) {
        return;
    }

    OBSPlugin *self = OBS_PLUGIN(user_data);
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    if (data == NULL) {
        return;
    }

    gchar *scene_name = NULL;
    int scene_item_id = -1;
    gboolean scene_item_enabled = FALSE;

    if (json_object_has_member(json, "eventData")) {
        JsonNode *event_node = json_object_get_member(json, "eventData");
        if (JSON_NODE_HOLDS_OBJECT(event_node)) {
            JsonObject *event_data = json_node_get_object(event_node);
            scene_name = (gchar *)json_object_get_string_value(event_data, "sceneName");
            scene_item_id = json_object_get_int_member(event_data, "sceneItemId");
            scene_item_enabled = json_object_get_boolean_value(event_data, "sceneItemEnabled");
        }
    }

    if (scene_name == NULL || scene_item_id < 0) {
        return;
    }

    /* Check if this event is for our scene and scene item */
    if (g_strcmp0(scene_name, data->scene_name) == 0 && scene_item_id == data->scene_item_id) {
        data->is_visible = scene_item_enabled;
        if (scene_item_enabled) {
            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_SELECTED);
        } else {
            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_NORMAL);
        }
    }
}

/* Callback to get initial state of scene item */
static void on_scene_item_list_for_state(JsonObject *json, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    if (self == NULL) {
        return;
    }

    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    if (data == NULL || data->scene_name == NULL || data->source_name == NULL) {
        return;
    }

    JsonObject *response_data = json;
    if (json_object_has_member(json, "responseData")) {
        JsonNode *response_node = json_object_get_member(json, "responseData");
        if (JSON_NODE_HOLDS_OBJECT(response_node)) {
            response_data = json_node_get_object(response_node);
        }
    }

    if (json_object_has_member(response_data, "sceneItems")) {
        JsonNode *node = json_object_get_member(response_data, "sceneItems");
        if (JSON_NODE_HOLDS_ARRAY(node)) {
            JsonArray *array = json_node_get_array(node);
            guint len = json_array_get_length(array);

            for (guint i = 0; i < len; i++) {
                JsonNode *item_node = json_array_get_element(array, i);
                if (JSON_NODE_HOLDS_OBJECT(item_node)) {
                    JsonObject *item_obj = json_node_get_object(item_node);
                    const char *source_name = json_object_get_string_value(item_obj, "sourceName");
                    int scene_item_id = (int)json_object_get_int_member(item_obj, "sceneItemId");
                    gboolean is_enabled =
                        json_object_get_boolean_value(item_obj, "sceneItemEnabled");

                    if (g_strcmp0(source_name, data->source_name) == 0) {
                        data->scene_item_id = scene_item_id;
                        data->is_visible = is_enabled;
                        if (is_enabled) {
                            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_SELECTED);
                        } else {
                            deck_plugin_set_state(DECK_PLUGIN(self), BUTTON_STATE_NORMAL);
                        }
                        break;
                    }
                }
            }
        }
    }
}

/* Lifecycle methods */
static void toggle_source_visibility_init(OBSPlugin *self) {
    ToggleSourceVisibilityData *data = g_new0(ToggleSourceVisibilityData, 1);
    data->scene_name = NULL;
    data->source_name = NULL;
    data->scene_item_id = -1;
    data->is_visible = FALSE;
    obs_plugin_set_action_data(self, data);

    ObsWs *ws = obs_ws_new();
    obs_ws_register_callback(ws, "SceneItemEnableStateChanged", on_scene_item_enable_changed, self);
}

static void toggle_source_visibility_destroy(OBSPlugin *self) {
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    if (data != NULL) {
        if (data->scene_name != NULL) {
            g_free(data->scene_name);
        }
        if (data->source_name != NULL) {
            g_free(data->source_name);
        }
        g_free(data);
        obs_plugin_set_action_data(self, NULL);
    }
}

/* Config UI structures and callbacks */
typedef struct {
    GtkComboBox *scene_combo;
    GtkComboBox *source_combo;
    gchar *selected_scene;
    gchar *selected_source;
} SceneSourceListData;

static void on_source_list(JsonObject *json, gpointer user_data) {
    SceneSourceListData *data = (SceneSourceListData *)user_data;

    if (data == NULL || data->source_combo == NULL || !GTK_IS_COMBO_BOX(data->source_combo)) {
        printf("DEBUG: on_source_list - Invalid data or combo\n");
        if (data != NULL) {
            g_free(data->selected_scene);
            g_free(data->selected_source);
            g_free(data);
        }
        return;
    }

    printf("DEBUG: on_source_list - Looking for source: '%s'\n",
           data->selected_source ? data->selected_source : "NULL");

    GtkTreeModel *model = gtk_combo_box_get_model(data->source_combo);
    GtkListStore *store = GTK_LIST_STORE(model);

    /* Clear existing items */
    gtk_list_store_clear(store);

    JsonObject *response_data = json;
    if (json_object_has_member(json, "responseData")) {
        JsonNode *response_node = json_object_get_member(json, "responseData");
        if (JSON_NODE_HOLDS_OBJECT(response_node)) {
            response_data = json_node_get_object(response_node);
        }
    }

    if (json_object_has_member(response_data, "sceneItems")) {
        JsonNode *node = json_object_get_member(response_data, "sceneItems");
        if (JSON_NODE_HOLDS_ARRAY(node)) {
            JsonArray *array = json_node_get_array(node);
            guint len = json_array_get_length(array);
            printf("DEBUG: on_source_list - Got %d sources\n", len);

            GtkTreeIter iter;
            gint active_index = -1;
            gint index = 0;
            for (guint i = 0; i < len; i++) {
                JsonNode *item_node = json_array_get_element(array, i);
                if (JSON_NODE_HOLDS_OBJECT(item_node)) {
                    JsonObject *item_obj = json_node_get_object(item_node);
                    if (json_object_has_member(item_obj, "sourceName")) {
                        const char *source_name =
                            json_object_get_string_value(item_obj, "sourceName");
                        gtk_list_store_append(store, &iter);
                        gtk_list_store_set(store, &iter, 0, source_name, -1);

                        printf("DEBUG:   Source %d: '%s'\n", index, source_name);

                        if (data->selected_source != NULL &&
                            g_strcmp0(source_name, data->selected_source) == 0) {
                            active_index = index;
                            printf("DEBUG:   -> MATCH at index %d\n", index);
                        }
                        index++;
                    }
                }
            }

            if (active_index >= 0) {
                printf("DEBUG: on_source_list - Setting active to %d\n", active_index);
                gtk_combo_box_set_active(data->source_combo, active_index);
            } else {
                printf("DEBUG: on_source_list - No match found for '%s'\n",
                       data->selected_source ? data->selected_source : "NULL");
            }
        }
    }

    g_free(data->selected_scene);
    g_free(data->selected_source);
    g_free(data);
}

typedef struct {
    OBSPlugin *plugin;
    GtkComboBox *source_combo;
    gchar *selected_source;
} SceneChangedData;

static void scene_changed_for_sources(GtkComboBox *widget, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        char *scene_name;
        GtkTreeModel *store = gtk_combo_box_get_model(widget);
        gtk_tree_model_get(store, &iter, 0, &scene_name, -1);

        printf("DEBUG: scene_changed_for_sources - Scene changed to: '%s'\n", scene_name);

        /* Check if this is the initial load or a user change */
        gboolean is_initial_load =
            (data->scene_name != NULL && g_strcmp0(data->scene_name, scene_name) == 0);

        if (data->scene_name != NULL) {
            g_free(data->scene_name);
        }
        data->scene_name = g_strdup(scene_name);

        /* Only clear source if this is a real scene change (not initial load) */
        if (!is_initial_load) {
            printf("DEBUG: scene_changed_for_sources - Clearing source (real change)\n");
            if (data->source_name != NULL) {
                g_free(data->source_name);
                data->source_name = NULL;
            }
            data->scene_item_id = -1;
        } else {
            printf("DEBUG: scene_changed_for_sources - Keeping source (initial load): '%s'\n",
                   data->source_name ? data->source_name : "NULL");
        }

        /* Clear the source combo */
        GtkWidget *source_combo = GTK_WIDGET(g_object_get_data(G_OBJECT(widget), "source_combo"));
        if (source_combo != NULL) {
            GtkTreeModel *source_model = gtk_combo_box_get_model(GTK_COMBO_BOX(source_combo));
            gtk_list_store_clear(GTK_LIST_STORE(source_model));
        }

        /* Get the source combo from the widget's user data */
        GtkComboBox *source_combo_box =
            GTK_COMBO_BOX(g_object_get_data(G_OBJECT(widget), "source_combo"));

        /* Update source combo for this scene */
        if (source_combo_box != NULL) {
            SceneSourceListData *list_data = g_new0(SceneSourceListData, 1);
            list_data->source_combo = source_combo_box;
            /* Pre-select source only on initial load */
            list_data->selected_source = is_initial_load ? g_strdup(data->source_name) : NULL;

            ObsWs *ws = obs_ws_new();
            obs_ws_get_scene_item_list(ws, scene_name, on_source_list, list_data);
        }

        g_free(scene_name);
    }
}

static void source_visibility_changed(GtkComboBox *widget, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        char *source_name;
        GtkTreeModel *store = gtk_combo_box_get_model(widget);
        gtk_tree_model_get(store, &iter, 0, &source_name, -1);

        printf("DEBUG: source_visibility_changed - Selected source: '%s'\n", source_name);

        if (data->source_name != NULL) {
            g_free(data->source_name);
        }
        data->source_name = g_strdup(source_name);
        data->scene_item_id = -1; /* Will be looked up on execution */

        g_free(source_name);
    }
}

static void on_scene_list(JsonObject *json, gpointer user_data) {
    SceneSourceListData *data = (SceneSourceListData *)user_data;

    if (data == NULL || data->scene_combo == NULL || !GTK_IS_COMBO_BOX(data->scene_combo)) {
        if (data != NULL) {
            g_free(data->selected_scene);
            g_free(data->selected_source);
            g_free(data);
        }
        return;
    }

    GtkTreeModel *model = gtk_combo_box_get_model(data->scene_combo);
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
                gtk_combo_box_set_active(data->scene_combo, active_index);
            }
        }
    }
}

static void toggle_source_visibility_config(OBSPlugin *self, GtkBox *parent) {
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);

    /* Scene selection */
    GtkWidget *scene_label = gtk_label_new("Scene");
    gchar *current_scene = g_strdup(data->scene_name);

    GtkListStore *scene_store = gtk_list_store_new(1, G_TYPE_STRING);

    GtkCellRenderer *scene_renderer = gtk_cell_renderer_text_new();
    g_object_set(scene_renderer, "height", 30, "width", 200, NULL);

    GtkWidget *scene_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(scene_store));
    g_signal_connect(G_OBJECT(scene_combo), "changed", G_CALLBACK(scene_changed_for_sources), self);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(scene_combo), scene_renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(scene_combo), GTK_CELL_RENDERER(scene_renderer),
                                   "text", 0, NULL);

    /* Source selection - declare source variables before scene_list_data */
    GtkWidget *source_label = gtk_label_new("Source");
    gchar *current_source = g_strdup(data->source_name);

    SceneSourceListData *scene_list_data = g_new0(SceneSourceListData, 1);
    scene_list_data->scene_combo = GTK_COMBO_BOX(scene_combo);
    scene_list_data->selected_scene = g_strdup(current_scene);
    scene_list_data->selected_source = g_strdup(current_source);

    ObsWs *ws = obs_ws_new();
    obs_ws_get_scenes(ws, on_scene_list, scene_list_data);

    GtkListStore *source_store = gtk_list_store_new(1, G_TYPE_STRING);

    GtkCellRenderer *source_renderer = gtk_cell_renderer_text_new();
    g_object_set(source_renderer, "height", 30, "width", 200, NULL);

    GtkWidget *source_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(source_store));
    g_signal_connect(G_OBJECT(source_combo), "changed", G_CALLBACK(source_visibility_changed),
                     self);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(source_combo), source_renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(source_combo),
                                   GTK_CELL_RENDERER(source_renderer), "text", 0, NULL);

    /* Store reference to source_combo in scene_combo so we can access it in the callback */
    g_object_set_data(G_OBJECT(scene_combo), "source_combo", source_combo);

    gtk_box_pack_start(parent, scene_label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, scene_combo, TRUE, FALSE, 5);

    /* If we have a scene selected, populate sources */
    if (data->scene_name != NULL) {
        SceneSourceListData *source_list_data = g_new0(SceneSourceListData, 1);
        source_list_data->source_combo = GTK_COMBO_BOX(source_combo);
        source_list_data->selected_scene = g_strdup(data->scene_name);
        source_list_data->selected_source = g_strdup(current_source);

        obs_ws_get_scene_item_list(ws, data->scene_name, on_source_list, source_list_data);
    }

    gtk_box_pack_start(parent, source_label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, source_combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(scene_store));
    g_object_unref(G_OBJECT(source_store));
    g_free(current_scene);
    g_free(current_source);
}

/* Execution */
static void on_scene_item_lookup(JsonObject *json, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    if (self == NULL) {
        printf("DEBUG: on_scene_item_lookup - self is NULL\n");
        return;
    }

    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    if (data == NULL || data->scene_name == NULL || data->source_name == NULL) {
        printf("DEBUG: on_scene_item_lookup - invalid data\n");
        return;
    }

    printf("DEBUG: on_scene_item_lookup - Looking for source '%s' in scene '%s'\n",
           data->source_name, data->scene_name);

    JsonObject *response_data = json;
    if (json_object_has_member(json, "responseData")) {
        JsonNode *response_node = json_object_get_member(json, "responseData");
        if (JSON_NODE_HOLDS_OBJECT(response_node)) {
            response_data = json_node_get_object(response_node);
        }
    }

    gboolean found = FALSE;
    if (json_object_has_member(response_data, "sceneItems")) {
        JsonNode *node = json_object_get_member(response_data, "sceneItems");
        if (JSON_NODE_HOLDS_ARRAY(node)) {
            JsonArray *array = json_node_get_array(node);
            guint len = json_array_get_length(array);
            printf("DEBUG: on_scene_item_lookup - Got %d scene items\n", len);

            for (guint i = 0; i < len; i++) {
                JsonNode *item_node = json_array_get_element(array, i);
                if (JSON_NODE_HOLDS_OBJECT(item_node)) {
                    JsonObject *item_obj = json_node_get_object(item_node);
                    const char *source_name = json_object_get_string_value(item_obj, "sourceName");
                    int scene_item_id = (int)json_object_get_int_member(item_obj, "sceneItemId");
                    gboolean is_enabled =
                        json_object_get_boolean_value(item_obj, "sceneItemEnabled");

                    printf("DEBUG:   Item %d: sourceName='%s', sceneItemId=%d, enabled=%d\n", i,
                           source_name ? source_name : "NULL", scene_item_id, is_enabled);

                    if (g_strcmp0(source_name, data->source_name) == 0) {
                        printf("DEBUG:   -> MATCH FOUND!\n");
                        data->scene_item_id = scene_item_id;
                        data->is_visible = is_enabled;

                        /* Toggle the visibility */
                        ObsWs *ws = obs_ws_new();
                        obs_ws_set_scene_item_enabled(ws, data->scene_name, data->scene_item_id,
                                                      !is_enabled);
                        found = TRUE;
                        break;
                    }
                }
            }
        }
    }

    if (!found) {
        printf("DEBUG: on_scene_item_lookup - Source '%s' NOT FOUND in scene '%s'\n",
               data->source_name, data->scene_name);
    }
}

static void toggle_source_visibility_exec(OBSPlugin *self) {
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    if (data == NULL || data->scene_name == NULL || data->source_name == NULL) {
        printf("DEBUG: Cannot exec - data=%p, scene_name=%s, source_name=%s\n", (void *)data,
               data ? (data->scene_name ? data->scene_name : "NULL") : "N/A",
               data ? (data->source_name ? data->source_name : "NULL") : "N/A");
        return;
    }

    printf("DEBUG: Exec toggle for scene='%s', source='%s', scene_item_id=%d, is_visible=%d\n",
           data->scene_name, data->source_name, data->scene_item_id, data->is_visible);

    /* If we don't know the scene item ID yet, look it up first */
    if (data->scene_item_id < 0) {
        printf("DEBUG: Looking up scene_item_id for source '%s' in scene '%s'\n", data->source_name,
               data->scene_name);
        ObsWs *ws = obs_ws_new();
        obs_ws_get_scene_item_list(ws, data->scene_name, on_scene_item_lookup, self);
    } else {
        /* Toggle the visibility */
        printf("DEBUG: Toggling visibility for scene_item_id=%d to %d\n", data->scene_item_id,
               !data->is_visible);
        ObsWs *ws = obs_ws_new();
        obs_ws_set_scene_item_enabled(ws, data->scene_name, data->scene_item_id, !data->is_visible);
    }
}

/* Serialization */
static void toggle_source_visibility_save(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    if (data != NULL) {
        if (data->scene_name != NULL) {
            g_key_file_set_string(key_file, group, "scene_name", data->scene_name);
        }
        if (data->source_name != NULL) {
            g_key_file_set_string(key_file, group, "source_name", data->source_name);
        }
    }
}

static void toggle_source_visibility_load(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ToggleSourceVisibilityData *data = obs_plugin_get_action_data(self);
    g_autofree char *scene_name = g_key_file_get_string(key_file, group, "scene_name", NULL);
    g_autofree char *source_name = g_key_file_get_string(key_file, group, "source_name", NULL);

    if (scene_name != NULL) {
        if (data->scene_name != NULL) {
            g_free(data->scene_name);
        }
        data->scene_name = g_strdup(scene_name);
    }

    if (source_name != NULL) {
        if (data->source_name != NULL) {
            g_free(data->source_name);
        }
        data->source_name = g_strdup(source_name);
        data->scene_item_id = -1; /* Will be looked up on next execution */
    }

    /* Get initial state */
    if (data->scene_name != NULL && data->source_name != NULL) {
        ObsWs *ws = obs_ws_new();
        obs_ws_get_scene_item_list(ws, data->scene_name, on_scene_item_list_for_state, self);
    }
}

/* VTable export */
const OBSActionVTable obs_toggle_source_visibility_vtable = {
    .init = toggle_source_visibility_init,
    .destroy = toggle_source_visibility_destroy,
    .config = toggle_source_visibility_config,
    .exec = toggle_source_visibility_exec,
    .save = toggle_source_visibility_save,
    .load = toggle_source_visibility_load,
    .render = NULL, /* Use default render */
};
