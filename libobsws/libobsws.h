#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

#ifndef __OBS_WS_H__
#define __OBS_WS_H__

G_BEGIN_DECLS

#define OBS_TYPE_WS obs_ws_get_type()
G_DECLARE_DERIVABLE_TYPE(ObsWs, obs_ws, OBS, WS, GObject)

typedef void (*result_callback)(JsonObject *, gpointer user_data);

struct _ObsWsClass {
    GObjectClass parent;

    /* class members */
    void (*heartbeat)(const gchar *profile, const gchar *scene);

    GThread *thread;
    struct wic_inst *inst;
    GHashTable *callbacks;
    GHashTable *callbacks_data;

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

/*
 * Method definitions.
 */
GType obs_ws_get_type();
ObsWs *obs_ws_new();
GList *obs_ws_get_scenes(ObsWs *, result_callback callback, gpointer user_data);
void obs_ws_set_current_scene(ObsWs *self, const char *scene);
void obs_ws_get_current_scene(ObsWs *self, result_callback callback, gpointer user_data);
void obs_ws_register_callback(ObsWs *self, const char *callbackId, result_callback callback,
                              gpointer user_data);
void obs_ws_set_password(const gchar *password);
GList *obs_ws_get_input_list(ObsWs *self, result_callback callback, gpointer user_data);
void obs_ws_toggle_input_mute(ObsWs *self, const char *input_name);
void obs_ws_get_input_mute(ObsWs *self, const char *input_name, result_callback callback,
                           gpointer user_data);
void obs_ws_toggle_record(ObsWs *self);
void obs_ws_get_record_status(ObsWs *self, result_callback callback, gpointer user_data);
void obs_ws_set_scene_item_enabled(ObsWs *self, const char *scene_name, int scene_item_id,
                                   gboolean enabled);
void obs_ws_get_scene_item_list(ObsWs *self, const char *scene_name, result_callback callback,
                                gpointer user_data);
void obs_ws_set_record_chapter_marker(ObsWs *self, const char *chapter_name);
void obs_ws_save_source_screenshot(ObsWs *self, const char *source_name, const char *image_format,
                                   const char *image_file_path, int image_width, int image_height);

const gchar *json_object_get_string_value(JsonObject *json, const gchar *key);
const gboolean json_object_get_boolean_value(JsonObject *json, const gchar *key);

G_END_DECLS

#endif // __OBS_WS_H__
