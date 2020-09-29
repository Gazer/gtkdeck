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

void media_exec(DeckPlugin *self) {
    int current_key = 0;
    gint exit_status;

    g_object_get(G_OBJECT(self), "media_key", &current_key, NULL);

    g_autofree gchar *command = g_strdup_printf("xvkbd -text \"\\[%s]\"", keys[current_key]);
    g_spawn_command_line_sync(command, NULL, NULL, &exit_status, NULL);
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