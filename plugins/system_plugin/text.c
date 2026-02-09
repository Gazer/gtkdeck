#include "../deck_plugin.h"

void text_changed(GtkEditable *editable, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);

    char *text = gtk_editable_get_chars(editable, 0, -1);
    g_object_set(G_OBJECT(self), "text", text, NULL);
    g_free(text);
}

void text_config(DeckPlugin *self, GtkBox *parent) {
    gchar *text;
    g_object_get(G_OBJECT(self), "text", &text, NULL);

    GtkBox *hbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8));

    GtkWidget *label = gtk_label_new("Text");
    // GTK_INPUT_PURPOSE_URL
    // GTK_INPUT_HINT_NO_SPELLCHECK
    GtkWidget *entry = gtk_entry_new();
    if (text != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry), text);
    }
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(text_changed), self);

    gtk_box_pack_start(hbox, label, FALSE, TRUE, 5);
    gtk_box_pack_start(hbox, entry, TRUE, TRUE, 5);

    gtk_box_pack_start(parent, GTK_WIDGET(hbox), TRUE, FALSE, 5);
}

static void run_command(const char *cmd) {
    gint exit_status;
    GError *error = NULL;
    if (!g_spawn_command_line_sync(cmd, NULL, NULL, &exit_status, &error)) {
        g_warning("Failed to run command '%s': %s", cmd, error->message);
        g_error_free(error);
    }
}

void text_exec(DeckPlugin *self) {
    gchar *text;
    g_object_get(G_OBJECT(self), "text", &text, NULL);

    if (text == NULL)
        return;

    // 1. Try wtype (Wayland - wlroots based like Sway/Hyprland)
    g_autofree char *wtype_path = g_find_program_in_path("wtype");
    if (wtype_path) {
        // wtype needs escaping for some chars, but basic usage: wtype "text"
        g_autofree char *cmd = g_strdup_printf("%s \"%s\"", wtype_path, text);
        run_command(cmd);
        g_free(text);
        return;
    }

    // 2. Try xdotool (X11 Standard)
    g_autofree char *xdotool_path = g_find_program_in_path("xdotool");
    if (xdotool_path) {
        g_autofree char *cmd = g_strdup_printf("%s type --delay 0 \"%s\"", xdotool_path, text);
        run_command(cmd);
        g_free(text);
        return;
    }

    // 3. Fallback to xvkbd (Legacy X11)
    g_autofree char *cmd = g_strdup_printf("xvkbd -text \"%s\"", text);
    run_command(cmd);
    g_free(text);
}

void text_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    gchar *text;
    g_object_get(G_OBJECT(self), "text", &text, NULL);

    if (text != NULL) {
        g_key_file_set_string(key_file, group, "text", text);
        g_free(text);
    }
}

void text_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    g_autofree char *text = g_key_file_get_string(key_file, group, "text", NULL);

    if (text != NULL) {
        g_object_set(G_OBJECT(self), "text", text, NULL);
    }
}
