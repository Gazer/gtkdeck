/* obs_action_types.h - Private header for OBS Plugin action implementations */
#ifndef __OBS_ACTION_TYPES_H__
#define __OBS_ACTION_TYPES_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

/* Forward declarations */
struct _OBSPlugin;
typedef struct _OBSPlugin OBSPlugin;

typedef struct _OBSActionVTable {
    /* Lifecycle */
    void (*init)(OBSPlugin *self);
    void (*destroy)(OBSPlugin *self);

    /* Plugin operations */
    void (*config)(OBSPlugin *self, GtkBox *parent);
    void (*exec)(OBSPlugin *self);
    void (*save)(OBSPlugin *self, const char *group, GKeyFile *key_file);
    void (*load)(OBSPlugin *self, const char *group, GKeyFile *key_file);
    void (*render)(OBSPlugin *self, cairo_t *cr, int width, int height);
} OBSActionVTable;

/* Action vtables declarations */
extern const OBSActionVTable obs_change_scene_vtable;
extern const OBSActionVTable obs_mute_source_vtable;
extern const OBSActionVTable obs_toggle_recording_vtable;

/* Helper to get/set action_data from plugin instance */
void obs_plugin_set_action_data(OBSPlugin *self, void *data);
void *obs_plugin_get_action_data(OBSPlugin *self);

/* Get vtable from plugin instance */
const OBSActionVTable *obs_plugin_get_vtable(OBSPlugin *self);

G_END_DECLS

#endif /* __OBS_ACTION_TYPES_H__ */
