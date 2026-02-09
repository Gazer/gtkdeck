#include "../deck_plugin.h"

static char *resources[] = {"system-previous",   "system-resume", "system-next",
                            "system-stop",       "system-mute",   "system-volume-up",
                            "system-volume-down"};

static void media_key_changed(GtkComboBox *widget, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);
    int key = gtk_combo_box_get_active(widget);

    g_object_set(G_OBJECT(self), "media_key", key, NULL);

    g_autofree gchar *resource =
        g_strdup_printf("/ar/com/p39/gtkdeck/plugins/%s.png", resources[key]);

    deck_plugin_set_image_from_resource(self, BUTTON_STATE_NORMAL, resource);
}

void media_config(DeckPlugin *self, GtkBox *parent) {
    GtkWidget *label = gtk_label_new("Action");
    int current_key = 0;

    g_object_get(G_OBJECT(self), "media_key", &current_key, NULL);

    GtkTreeIter iter;
    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Previous Track", -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Play / Pause", -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Next Track", -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Stop", -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Mute", -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Volume Up", -1);
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, "Volume Down", -1);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "height", 30, "width", 200, NULL);

    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(media_key_changed), self);

    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current_key);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), GTK_CELL_RENDERER(renderer), "text", 0,
                                   NULL);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(store));
}

static char *keys[] = {"XF86AudioPrev", "XF86AudioPlay",        "XF86AudioNext",       "XF86Stop",
                       "XF86AudioMute", "XF86AudioRaiseVolume", "XF86AudioLowerVolume"};

static void run_command(const char *cmd) {
    gint exit_status;
    GError *error = NULL;
    if (!g_spawn_command_line_sync(cmd, NULL, NULL, &exit_status, &error)) {
        g_warning("Failed to run command '%s': %s", cmd, error->message);
        g_error_free(error);
    }
}

void media_exec(DeckPlugin *self) {
    int current_key = 0;
    g_object_get(G_OBJECT(self), "media_key", &current_key, NULL);

    // Check for playerctl for media keys (0-3)
    if (current_key <= 3) {
        g_autofree char *playerctl_path = g_find_program_in_path("playerctl");
        if (playerctl_path) {
            const char *args[] = {"previous", "play-pause", "next", "stop"};
            g_autofree char *cmd = g_strdup_printf("%s %s", playerctl_path, args[current_key]);
            run_command(cmd);
            return;
        }
    }

    // Check for volume keys (4-6)
    if (current_key >= 4) {
        // Try wpctl (WirePlumber)
        g_autofree char *wpctl_path = g_find_program_in_path("wpctl");
        if (wpctl_path) {
            const char *args[] = {"set-mute @DEFAULT_AUDIO_SINK@ toggle",
                                  "set-volume @DEFAULT_AUDIO_SINK@ 5%+",
                                  "set-volume @DEFAULT_AUDIO_SINK@ 5%-"};
            g_autofree char *cmd = g_strdup_printf("%s %s", wpctl_path, args[current_key - 4]);
            run_command(cmd);
            return;
        }

        // Try pactl (PulseAudio)
        g_autofree char *pactl_path = g_find_program_in_path("pactl");
        if (pactl_path) {
            const char *args[] = {"set-sink-mute @DEFAULT_SINK@ toggle",
                                  "set-sink-volume @DEFAULT_SINK@ +5%",
                                  "set-sink-volume @DEFAULT_SINK@ -5%"};
            g_autofree char *cmd = g_strdup_printf("%s %s", pactl_path, args[current_key - 4]);
            run_command(cmd);
            return;
        }

        // Try amixer (ALSA)
        g_autofree char *amixer_path = g_find_program_in_path("amixer");
        if (amixer_path) {
            const char *args[] = {"set Master toggle", "set Master 5%+", "set Master 5%-"};
            g_autofree char *cmd = g_strdup_printf("%s %s", amixer_path, args[current_key - 4]);
            run_command(cmd);
            return;
        }
    }

    // Fallback to xvkbd
    g_autofree gchar *command = g_strdup_printf("xvkbd -text \"\\[%s]\"", keys[current_key]);
    run_command(command);
}

void media_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    int media_key = 0;
    g_object_get(G_OBJECT(self), "media_key", &media_key, NULL);

    g_key_file_set_integer(key_file, group, "media_key", media_key);
}

void media_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    int media_key = g_key_file_get_integer(key_file, group, "media_key", NULL);

    g_object_set(G_OBJECT(self), "media_key", media_key, NULL);
}