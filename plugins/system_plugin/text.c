#include "../deck_plugin.h"

void text_changed(GtkEditable *editable, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);

    char *text = gtk_editable_get_chars(editable, 0, -1);
    g_object_set(G_OBJECT(self), "url", text, NULL);
    g_free(text);
}

void text_config(DeckPlugin *self, GtkBox *parent) {
    gchar *url;
    g_object_get(G_OBJECT(self), "url", &url, NULL);

    GtkBox *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    GtkWidget *label = gtk_label_new("Text");
    // GTK_INPUT_PURPOSE_URL
    // GTK_INPUT_HINT_NO_SPELLCHECK
    GtkWidget *entry = gtk_entry_new();
    if (url != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry), url);
    }
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(text_changed), self);

    gtk_box_pack_start(hbox, label, FALSE, TRUE, 5);
    gtk_box_pack_start(hbox, entry, TRUE, TRUE, 5);

    gtk_box_pack_start(parent, GTK_WIDGET(hbox), TRUE, FALSE, 5);
}

void text_exec(DeckPlugin *self) {
    gchar *url;
    gint exit_status;

    g_object_get(G_OBJECT(self), "url", &url, NULL);

    char *command = g_strdup_printf("xvkbd -text \"%s\"", url);
    // TODO
    g_spawn_command_line_sync(command, NULL, NULL, &exit_status, NULL);

    g_free(command);
    g_free(url);
}

void text_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    gchar *url;
    g_object_get(G_OBJECT(self), "url", &url, NULL);

    if (url != NULL) {
        g_key_file_set_string(key_file, group, "text", url);
        g_free(url);
    }
}

void text_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    g_autofree char *url = g_key_file_get_string(key_file, group, "text", NULL);

    if (url != NULL) {
        g_object_set(G_OBJECT(self), "url", url, NULL);
    }
}