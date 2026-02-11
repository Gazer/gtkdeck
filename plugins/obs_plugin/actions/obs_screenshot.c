#include "../../libobsws/libobsws.h"
#include "../obs_action_types.h"
#include "../obs_plugin.h"

/* Private data for Screenshot action */
typedef struct {
    gchar *save_path;
    gchar *image_format;
    int image_width;
    int image_height;
} ScreenshotData;

/* Lifecycle methods */
static void screenshot_init(OBSPlugin *self) {
    ScreenshotData *data = g_new0(ScreenshotData, 1);
    data->save_path = NULL;
    data->image_format = g_strdup("png");
    data->image_width = 0;  /* 0 = use source size */
    data->image_height = 0; /* 0 = use source size */
    obs_plugin_set_action_data(self, data);
}

static void screenshot_destroy(OBSPlugin *self) {
    ScreenshotData *data = obs_plugin_get_action_data(self);
    if (data != NULL) {
        if (data->save_path != NULL) {
            g_free(data->save_path);
        }
        if (data->image_format != NULL) {
            g_free(data->image_format);
        }
        g_free(data);
        obs_plugin_set_action_data(self, NULL);
    }
}

/* Config UI */
typedef struct {
    OBSPlugin *plugin;
    GtkWidget *path_entry;
} ScreenshotConfigData;

static void on_file_set(GtkFileChooserButton *chooser, gpointer user_data) {
    ScreenshotConfigData *config_data = (ScreenshotConfigData *)user_data;
    OBSPlugin *self = config_data->plugin;
    ScreenshotData *data = obs_plugin_get_action_data(self);

    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser));
    if (filename != NULL) {
        if (data->save_path != NULL) {
            g_free(data->save_path);
        }
        data->save_path = filename;
    }
}

static void on_format_changed(GtkComboBox *widget, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    ScreenshotData *data = obs_plugin_get_action_data(self);
    GtkTreeIter iter;

    if (gtk_combo_box_get_active_iter(widget, &iter)) {
        char *format;
        GtkTreeModel *store = gtk_combo_box_get_model(widget);
        gtk_tree_model_get(store, &iter, 0, &format, -1);

        if (data->image_format != NULL) {
            g_free(data->image_format);
        }
        data->image_format = g_strdup(format);
        g_free(format);
    }
}

static void screenshot_config(OBSPlugin *self, GtkBox *parent) {
    ScreenshotData *data = obs_plugin_get_action_data(self);

    /* Save path */
    GtkWidget *path_label = gtk_label_new("Save Path (optional)");
    gtk_box_pack_start(parent, path_label, TRUE, FALSE, 5);

    GtkWidget *path_chooser = gtk_file_chooser_button_new("Select Screenshot Location",
                                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    if (data->save_path != NULL) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(path_chooser), data->save_path);
    }

    ScreenshotConfigData *config_data = g_new0(ScreenshotConfigData, 1);
    config_data->plugin = self;
    config_data->path_entry = path_chooser;
    g_signal_connect(G_OBJECT(path_chooser), "file-set", G_CALLBACK(on_file_set), config_data);
    gtk_box_pack_start(parent, path_chooser, TRUE, FALSE, 5);

    /* Image format */
    GtkWidget *format_label = gtk_label_new("Image Format");
    gtk_box_pack_start(parent, format_label, TRUE, FALSE, 5);

    GtkListStore *format_store = gtk_list_store_new(1, G_TYPE_STRING);
    GtkTreeIter iter;
    gtk_list_store_append(format_store, &iter);
    gtk_list_store_set(format_store, &iter, 0, "png", -1);
    gtk_list_store_append(format_store, &iter);
    gtk_list_store_set(format_store, &iter, 0, "jpeg", -1);
    gtk_list_store_append(format_store, &iter);
    gtk_list_store_set(format_store, &iter, 0, "bmp", -1);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkWidget *format_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(format_store));
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(format_combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(format_combo), GTK_CELL_RENDERER(renderer),
                                   "text", 0, NULL);
    g_signal_connect(G_OBJECT(format_combo), "changed", G_CALLBACK(on_format_changed), self);

    /* Set active format */
    if (data->image_format != NULL) {
        if (g_strcmp0(data->image_format, "png") == 0) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 0);
        } else if (g_strcmp0(data->image_format, "jpeg") == 0) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 1);
        } else if (g_strcmp0(data->image_format, "bmp") == 0) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 2);
        }
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 0);
    }

    gtk_box_pack_start(parent, format_combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(format_store));

    /* Info label */
    GtkWidget *info_label = gtk_label_new("Screenshot will be saved to the specified path\n"
                                          "or to the default OBS screenshot directory.");
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_box_pack_start(parent, info_label, TRUE, FALSE, 10);
}

/* Execution */
static void screenshot_exec(OBSPlugin *self) {
    ScreenshotData *data = obs_plugin_get_action_data(self);

    ObsWs *ws = obs_ws_new();
    /* Use current program scene as the source name */
    obs_ws_save_source_screenshot(ws, "", data->image_format, data->save_path, data->image_width,
                                  data->image_height);
}

/* Serialization */
static void screenshot_save(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ScreenshotData *data = obs_plugin_get_action_data(self);
    if (data != NULL) {
        if (data->save_path != NULL) {
            g_key_file_set_string(key_file, group, "save_path", data->save_path);
        }
        if (data->image_format != NULL) {
            g_key_file_set_string(key_file, group, "image_format", data->image_format);
        }
    }
}

static void screenshot_load(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ScreenshotData *data = obs_plugin_get_action_data(self);
    g_autofree char *save_path = g_key_file_get_string(key_file, group, "save_path", NULL);
    g_autofree char *image_format = g_key_file_get_string(key_file, group, "image_format", NULL);

    if (save_path != NULL) {
        if (data->save_path != NULL) {
            g_free(data->save_path);
        }
        data->save_path = g_strdup(save_path);
    }

    if (image_format != NULL) {
        if (data->image_format != NULL) {
            g_free(data->image_format);
        }
        data->image_format = g_strdup(image_format);
    }
}

/* VTable export */
const OBSActionVTable obs_screenshot_vtable = {
    .init = screenshot_init,
    .destroy = screenshot_destroy,
    .config = screenshot_config,
    .exec = screenshot_exec,
    .save = screenshot_save,
    .load = screenshot_load,
    .render = NULL, /* Use default render */
};
