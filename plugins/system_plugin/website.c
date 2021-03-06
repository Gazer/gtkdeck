#include "../deck_plugin.h"

void website_changed(GtkEditable *editable, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(user_data);

    char *text = gtk_editable_get_chars(editable, 0, -1);
    g_object_set(G_OBJECT(self), "url", text, NULL);
    g_free(text);
}

void website_config(DeckPlugin *self, GtkBox *parent) {
    gchar *url;
    g_object_get(G_OBJECT(self), "url", &url, NULL);

    GtkWidget *label = gtk_label_new("URL");
    // GTK_INPUT_PURPOSE_URL
    // GTK_INPUT_HINT_NO_SPELLCHECK
    GtkWidget *entry = gtk_entry_new();
    if (url != NULL) {
        gtk_entry_set_text(GTK_ENTRY(entry), url);
    }
    g_signal_connect(G_OBJECT(entry), "changed", G_CALLBACK(website_changed), self);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, entry, TRUE, FALSE, 5);
}

void website_exec(DeckPlugin *self) {
    gchar *url;

    g_object_get(G_OBJECT(self), "url", &url, NULL);

    if (g_app_info_launch_default_for_uri(url, NULL, NULL)) {
        printf("Open web url: %s\n", url);
    }

    g_free(url);
}

void website_save(DeckPlugin *self, char *group, GKeyFile *key_file) {
    gchar *url;
    g_object_get(G_OBJECT(self), "url", &url, NULL);

    if (url != NULL) {
        g_key_file_set_string(key_file, group, "url", url);
        g_free(url);
    }
}

void website_load(DeckPlugin *self, char *group, GKeyFile *key_file) {
    g_autofree char *url = g_key_file_get_string(key_file, group, "url", NULL);

    if (url != NULL) {
        g_object_set(G_OBJECT(self), "url", url, NULL);
    }
}