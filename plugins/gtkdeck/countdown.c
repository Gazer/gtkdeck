#include "countdown.h"
#include <gtk/gtk.h>

typedef struct {
    int duration;
    char *sound_file;
    int remaining;
    guint timer_id;
    char *original_label;
} CountdownData;

static void countdown_data_free(gpointer data) {
    CountdownData *cdata = (CountdownData *)data;
    if (cdata->sound_file)
        g_free(cdata->sound_file);
    if (cdata->original_label)
        g_free(cdata->original_label);
    if (cdata->timer_id > 0)
        g_source_remove(cdata->timer_id);
    g_free(cdata);
}

static CountdownData *get_countdown_data(DeckPlugin *self) {
    CountdownData *data = g_object_get_data(G_OBJECT(self), "countdown_data");
    if (!data) {
        data = g_new0(CountdownData, 1);
        data->duration = 60;
        g_object_set_data_full(G_OBJECT(self), "countdown_data", data, countdown_data_free);
    }
    return data;
}

void on_duration_changed(GtkSpinButton *spin_button, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);
    CountdownData *data = get_countdown_data(self);
    data->duration = gtk_spin_button_get_value_as_int(spin_button);
}

void on_sound_file_set(GtkFileChooserButton *widget, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);
    CountdownData *data = get_countdown_data(self);
    if (data->sound_file)
        g_free(data->sound_file);
    data->sound_file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
}

void countdown_config(DeckPlugin *self, GtkBox *parent) {
    CountdownData *data = get_countdown_data(self);

    GtkWidget *hbox_duration = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label_duration = gtk_label_new("Duration (s):");
    GtkWidget *spin_duration = gtk_spin_button_new_with_range(1, 3600, 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_duration), data->duration);
    g_signal_connect(spin_duration, "value-changed", G_CALLBACK(on_duration_changed), self);

    gtk_box_pack_start(GTK_BOX(hbox_duration), label_duration, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_duration), spin_duration, TRUE, TRUE, 0);
    gtk_box_pack_start(parent, hbox_duration, FALSE, FALSE, 5);

    GtkWidget *hbox_sound = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *label_sound = gtk_label_new("Sound:");
    GtkWidget *file_chooser =
        gtk_file_chooser_button_new("Select Sound File", GTK_FILE_CHOOSER_ACTION_OPEN);
    if (data->sound_file) {
        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(file_chooser), data->sound_file);
    }

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Audio Files");
    gtk_file_filter_add_mime_type(filter, "audio/*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), filter);

    g_signal_connect(file_chooser, "file-set", G_CALLBACK(on_sound_file_set), self);

    gtk_box_pack_start(GTK_BOX(hbox_sound), label_sound, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_sound), file_chooser, TRUE, TRUE, 0);
    gtk_box_pack_start(parent, hbox_sound, FALSE, FALSE, 5);
}

static gboolean countdown_tick(gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);
    CountdownData *data = get_countdown_data(self);

    if (data->remaining > 0) {
        data->remaining--;

        int min = data->remaining / 60;
        int sec = data->remaining % 60;
        char *time_str = g_strdup_printf("%02d:%02d", min, sec);
        g_object_set(self, "label", time_str, NULL);
        g_free(time_str);

        return TRUE;
    } else {
        if (data->sound_file) {
            g_autofree char *quoted_file = g_shell_quote(data->sound_file);
            g_autofree char *cmd = NULL;

            // Try different players in order of preference
            if (g_find_program_in_path("mpv")) {
                cmd = g_strdup_printf("mpv --no-terminal %s", quoted_file);
            } else if (g_find_program_in_path("ffplay")) {
                cmd = g_strdup_printf("ffplay -nodisp -autoexit %s", quoted_file);
            } else if (g_find_program_in_path("mpg123")) {
                cmd = g_strdup_printf("mpg123 %s", quoted_file);
            } else if (g_find_program_in_path("paplay")) {
                cmd = g_strdup_printf("paplay %s", quoted_file);
            } else {
                // Fallback to gst-launch if nothing else matches
                g_autofree char *uri = g_filename_to_uri(data->sound_file, NULL, NULL);
                if (uri) {
                    cmd = g_strdup_printf("gst-launch-1.0 playbin uri=%s", uri);
                }
            }

            if (cmd) {
                g_spawn_command_line_async(cmd, NULL);
            }
        }

        if (data->original_label) {
            g_object_set(self, "label", data->original_label, NULL);
        } else {
            g_object_set(self, "label", "", NULL);
        }

        data->timer_id = 0;
        return FALSE;
    }
}

void countdown_exec(DeckPlugin *self) {
    CountdownData *data = get_countdown_data(self);

    if (data->timer_id > 0) {
        g_source_remove(data->timer_id);
        data->timer_id = 0;

        if (data->original_label) {
            g_object_set(self, "label", data->original_label, NULL);
        }
    } else {
        data->remaining = data->duration;

        gchar *current_label = NULL;
        g_object_get(self, "label", &current_label, NULL);
        if (data->original_label)
            g_free(data->original_label);
        data->original_label = current_label;

        int min = data->remaining / 60;
        int sec = data->remaining % 60;
        char *time_str = g_strdup_printf("%02d:%02d", min, sec);
        g_object_set(self, "label", time_str, NULL);
        g_free(time_str);

        data->timer_id = g_timeout_add(1000, countdown_tick, self);
    }
}

void countdown_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    CountdownData *data = get_countdown_data(self);
    g_key_file_set_integer(key_file, group, "countdown_duration", data->duration);
    if (data->sound_file) {
        g_key_file_set_string(key_file, group, "countdown_sound", data->sound_file);
    }
}

void countdown_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    CountdownData *data = get_countdown_data(self);
    if (g_key_file_has_key(key_file, group, "countdown_duration", NULL)) {
        data->duration = g_key_file_get_integer(key_file, group, "countdown_duration", NULL);
    }
    if (g_key_file_has_key(key_file, group, "countdown_sound", NULL)) {
        if (data->sound_file)
            g_free(data->sound_file);
        data->sound_file = g_key_file_get_string(key_file, group, "countdown_sound", NULL);
    }
}
