#include "../../libobsws/libobsws.h"
#include "../obs_action_types.h"
#include "../obs_plugin.h"

/* Private data for Chapter Marker action */
typedef struct {
    gchar *chapter_name;
} ChapterMarkerData;

/* Lifecycle methods */
static void chapter_marker_init(OBSPlugin *self) {
    ChapterMarkerData *data = g_new0(ChapterMarkerData, 1);
    data->chapter_name = NULL;
    obs_plugin_set_action_data(self, data);
}

static void chapter_marker_destroy(OBSPlugin *self) {
    ChapterMarkerData *data = obs_plugin_get_action_data(self);
    if (data != NULL) {
        if (data->chapter_name != NULL) {
            g_free(data->chapter_name);
        }
        g_free(data);
        obs_plugin_set_action_data(self, NULL);
    }
}

/* Config UI */
static void chapter_name_changed(GtkEditable *editable, gpointer user_data) {
    OBSPlugin *self = OBS_PLUGIN(user_data);
    ChapterMarkerData *data = obs_plugin_get_action_data(self);

    gchar *text = gtk_editable_get_chars(editable, 0, -1);
    if (data->chapter_name != NULL) {
        g_free(data->chapter_name);
    }
    data->chapter_name = g_strdup(text);
    g_free(text);
}

static void chapter_marker_config(OBSPlugin *self, GtkBox *parent) {
    ChapterMarkerData *data = obs_plugin_get_action_data(self);

    GtkWidget *label = gtk_label_new("Chapter Name (optional)");
    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter chapter name...");
    if (data->chapter_name != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry), data->chapter_name);
    }
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(chapter_name_changed), self);
    gtk_box_pack_start(parent, entry, TRUE, FALSE, 5);
}

/* Execution */
static void chapter_marker_exec(OBSPlugin *self) {
    ChapterMarkerData *data = obs_plugin_get_action_data(self);

    ObsWs *ws = obs_ws_new();
    obs_ws_set_record_chapter_marker(ws, data->chapter_name);
}

/* Serialization */
static void chapter_marker_save(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ChapterMarkerData *data = obs_plugin_get_action_data(self);
    if (data != NULL && data->chapter_name != NULL) {
        g_key_file_set_string(key_file, group, "chapter_name", data->chapter_name);
    }
}

static void chapter_marker_load(OBSPlugin *self, const char *group, GKeyFile *key_file) {
    ChapterMarkerData *data = obs_plugin_get_action_data(self);
    g_autofree char *chapter_name = g_key_file_get_string(key_file, group, "chapter_name", NULL);

    if (chapter_name != NULL) {
        if (data->chapter_name != NULL) {
            g_free(data->chapter_name);
        }
        data->chapter_name = g_strdup(chapter_name);
    }
}

/* VTable export */
const OBSActionVTable obs_chapter_marker_vtable = {
    .init = chapter_marker_init,
    .destroy = chapter_marker_destroy,
    .config = chapter_marker_config,
    .exec = chapter_marker_exec,
    .save = chapter_marker_save,
    .load = chapter_marker_load,
    .render = NULL, /* Use default render */
};
