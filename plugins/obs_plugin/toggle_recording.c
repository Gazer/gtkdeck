#include "../../libobsws/libobsws.h"
#include "../deck_plugin.h"

void toggle_recording_config(DeckPlugin *self, GtkBox *parent) {
    GtkWidget *label = gtk_label_new("Toggle OBS Recording");
    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
}

void toggle_recording_exec(DeckPlugin *self) {
    ObsWs *ws = obs_ws_new();
    obs_ws_toggle_record(ws);
}

void toggle_recording_save(DeckPlugin *self, char *group, GKeyFile *key_file) {}

void toggle_recording_load(DeckPlugin *self, char *group, GKeyFile *key_file) {}
