#include "../deck_plugin.h"

void change_scene_config(DeckPlugin *self, GtkBox *parent) {
    GtkWidget *label = gtk_label_new("Scene");
    g_autofree gchar *current_scene;

    g_object_get(G_OBJECT(self), "scene", &current_scene, NULL);

    GtkTreeIter iter;
    GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);

    // gtk_list_store_append(store, &iter);
    // gtk_list_store_set(store, &iter, 0, "Previous Track", -1);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "height", 30, "width", 200, NULL);

    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    // g_signal_connect(G_OBJECT(combo), "changed", G_CALLBACK(media_key_changed), self);

    // gtk_combo_box_set_active(GTK_COMBO_BOX(combo), current_key);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), GTK_CELL_RENDERER(renderer), "text", 0,
                                   NULL);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, combo, TRUE, FALSE, 5);

    g_object_unref(G_OBJECT(store));
}

void change_scene_exec(DeckPlugin *self) {}
void change_scene_save(DeckPlugin *self, char *group, GKeyFile *key_file) {}
void change_scene_load(DeckPlugin *self, char *group, GKeyFile *key_file) {}