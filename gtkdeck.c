#include "deck_plugin.h"
#include "streamdeck.h"
#include "test_plugin.h"
#include <gtk/gtk.h>
#include <unistd.h>

void save_config(StreamDeck *deck);

GList *plugin_list = NULL;
GtkBox *config_row;

DeckPlugin *BUTTON_ACTION[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static GtkTargetEntry entries[] = {{"text/plain", GTK_TARGET_SAME_APP, 0}};

struct DATA {
    DeckPlugin *plugin;
    int action;
};

void on_drag_data_received(GtkWidget *wgt, GdkDragContext *context, int x, int y,
                           GtkSelectionData *sdata, guint info, guint time, gpointer userdata);

void on_drag_data_get(GtkWidget *widget, GdkDragContext *drag_context, GtkSelectionData *sdata,
                      guint info, guint time, gpointer user_data) {
    GtkTreeSelection *selector;
    GtkTreeIter iter;
    GtkTreeModel *list_store;
    gboolean rv;

    /* Get the selector widget from the treeview in question */
    selector = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

    /* Get the tree model (list_store) and initialise the iterator */
    rv = gtk_tree_selection_get_selected(selector, &list_store, &iter);

    /* This shouldn't really happen, but just in case */
    if (rv == FALSE) {
        printf(" No row selected\n");
        return;
    }

    GValue value = {0};
    gpointer pointer;
    gtk_tree_model_get_value(list_store, &iter, 1, &value);
    pointer = g_value_get_pointer(&value);
    g_value_unset(&value);
    if (pointer == NULL) {
        // TODO: Header should not be dragable
        // Header, ignore
        return;
    }

    gtk_tree_model_get_value(list_store, &iter, 2, &value);
    int action = g_value_get_uint(&value);
    g_value_unset(&value);

    struct DATA *tmp = g_malloc0(sizeof(struct DATA));
    tmp->plugin = DECK_PLUGIN(pointer);
    tmp->action = action;

    gtk_selection_data_set(sdata, gdk_atom_intern("struct DATA pointer", FALSE), 8, (void *)&tmp,
                           sizeof(struct DATA));
}

void init_plugin_tree(GtkTreeView *list) {
    GList *iter = plugin_list;
    GtkTreeStore *treestore;
    GtkTreeIter item;
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Name");
    gtk_tree_view_append_column(list, col);

    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_attributes(col, renderer, "text", 0, NULL);

    treestore = gtk_tree_store_new(3, G_TYPE_STRING, G_TYPE_POINTER, G_TYPE_UINT);

    while (iter != NULL) {
        DeckPlugin *plugin = DECK_PLUGIN(iter->data);
        GtkTreeIter child;
        DeckPluginInfo *info = deck_plugin_get_info(plugin);

        gtk_tree_store_append(treestore, &item, NULL);
        gtk_tree_store_set(treestore, &item, 0, g_strdup(info->name), 1, NULL, 2, 0, -1);

        for (int i = 0; i < info->actions_count; i++) {
            DeckPluginAction action = info->actions[i];

            gtk_tree_store_append(treestore, &child, &item);
            gtk_tree_store_set(treestore, &child, 0, g_strdup(action.name), 1, plugin, 2, i, -1);
        }

        iter = iter->next;
    }

    gtk_drag_source_set(GTK_WIDGET(list), GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_COPY);
    g_signal_connect(list, "drag-data-get", G_CALLBACK(on_drag_data_get), NULL);
    gtk_tree_view_set_model(list, GTK_TREE_MODEL(treestore));
}

GtkWidget *grid_button(int k) {
    if (BUTTON_ACTION[k] == NULL) {
        return gtk_button_new_with_label("X");
    } else {
        cairo_surface_t *surface = deck_plugin_get_surface(BUTTON_ACTION[k]);
        return gtk_image_new_from_surface(surface);
    }
}

void pick_button_image(GtkButton *button, gpointer data) {
    DeckPlugin *plugin = DECK_PLUGIN(data);

    GtkFileFilter *filter = gtk_file_filter_new();
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
    gint res;

    gtk_file_filter_add_mime_type(filter, "image/*");

    dialog = gtk_file_chooser_dialog_new("Open File", NULL, action, "_Cancel", GTK_RESPONSE_CANCEL,
                                         "_Open", GTK_RESPONSE_ACCEPT, NULL);

    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *filename;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        filename = gtk_file_chooser_get_filename(chooser);

        deck_plugin_set_preview_from_file(plugin, filename);

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

void show_config(GtkButton *button, gpointer data) {
    GList *children;
    GtkWidget *config_area;
    DeckPlugin *plugin = DECK_PLUGIN(g_object_get_data(G_OBJECT(button), "plugin"));

    printf("Need to show button config\n");

    // Show Preview at the left
    children = gtk_container_get_children(GTK_CONTAINER(config_row));
    while (children != NULL) {
        GtkWidget *preview_widget = GTK_WIDGET(children->data);
        gtk_widget_destroy(preview_widget);
        children = children->next;
    }

    // Add Image picker button
    cairo_surface_t *surface = deck_plugin_get_surface(plugin);
    GtkWidget *plugin_preview = gtk_image_new_from_surface(surface);
    GtkWidget *pick_image_button = gtk_button_new();
    gtk_container_add(GTK_CONTAINER(pick_image_button), plugin_preview);
    g_signal_connect(G_OBJECT(pick_image_button), "clicked", G_CALLBACK(pick_button_image), plugin);

    gtk_box_pack_start(GTK_BOX(config_row), pick_image_button, FALSE, TRUE, 5);

    config_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(config_row), GTK_WIDGET(config_area), TRUE, TRUE, 5);

    // Show Plugin properties at the right
    deck_plugin_get_config_widget(DECK_PLUGIN(plugin), GTK_BOX(config_area));

    gtk_widget_show_all(GTK_WIDGET(config_row));
}

void on_deck_preview_update_device(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    StreamDeck *deck = STREAM_DECK(user_data);
    DeckPlugin *self = DECK_PLUGIN(gobject);
    cairo_surface_t *surface = deck_plugin_get_surface(self);
    int key = GPOINTER_TO_UINT(g_object_get_data(gobject, "key"));

    stream_deck_set_image_from_surface(deck, key, surface);
}

void on_deck_preview_update_app(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(gobject);
    cairo_surface_t *surface = deck_plugin_get_surface(self);

    gtk_image_set_from_surface(GTK_IMAGE(user_data), surface);
}

void on_deck_key_changed(GObject *gobject, int key, gpointer user_data) {
    DeckPlugin *plugin = BUTTON_ACTION[key];

    if (plugin != NULL) {
        deck_plugin_exec(plugin);
    }
}

void add_plugin_button(GtkGrid *grid, int key, StreamDeck *deck, DeckPlugin *plugin, int left,
                       int top) {
    GtkWidget *box = gtk_button_new();
    g_object_set_data(G_OBJECT(plugin), "key", GINT_TO_POINTER(key));

    GtkWidget *widget = grid_button(key);

    // If plugin updates, send the new image to the device
    g_signal_connect(plugin, "notify::preview", G_CALLBACK(on_deck_preview_update_device), deck);
    g_signal_connect(plugin, "notify::preview", G_CALLBACK(on_deck_preview_update_app), widget);

    gtk_drag_dest_set(box, GTK_DEST_DEFAULT_ALL, entries, 1, GDK_ACTION_COPY);
    g_signal_connect(box, "drag-data-received", G_CALLBACK(on_drag_data_received), grid);
    g_object_set_data(G_OBJECT(box), "plugin", plugin);
    g_signal_connect(box, "clicked", G_CALLBACK(show_config), deck);

    gtk_container_add(GTK_CONTAINER(box), widget);

    gtk_grid_attach(grid, box, left, top, 1, 1);
    gtk_widget_show_all(box);
}

void on_drag_data_received(GtkWidget *wgt, GdkDragContext *context, int x, int y,
                           GtkSelectionData *sdata, guint info, guint time, gpointer userdata) {
    GtkGrid *grid = GTK_GRID(userdata);
    StreamDeck *deck = STREAM_DECK(g_object_get_data(G_OBJECT(grid), "deck"));

    struct DATA *data = NULL;
    const guchar *my_data = gtk_selection_data_get_data(sdata);
    memcpy(&data, my_data, sizeof(data));

    for (int top = 0; top < 3; top++) {
        for (int left = 0; left < 5; left++) {
            GtkWidget *cell = gtk_grid_get_child_at(grid, left, top);

            if (cell == wgt) {
                int key = left + 5 * top;

                if (BUTTON_ACTION[key] != NULL) {
                    g_object_unref(G_OBJECT(BUTTON_ACTION[key]));
                }

                DeckPlugin *plugin = deck_plugin_new_with_action(data->plugin, data->action);
                BUTTON_ACTION[key] = plugin;

                gtk_widget_destroy(cell);
                add_plugin_button(grid, key, deck, plugin, left, top);
            }
        }
    }

    save_config(deck);

    g_free(data);
}

void init_button_grid(GtkGrid *grid, StreamDeck *deck) {
    g_object_set_data(G_OBJECT(grid), "deck", deck);

    g_signal_connect(deck, "key_pressed", G_CALLBACK(on_deck_key_changed), NULL);

    for (int top = 0; top < 3; top++) {
        for (int left = 0; left < 5; left++) {
            int key = left + 5 * top;

            add_plugin_button(grid, key, deck, BUTTON_ACTION[key], left, top);
        }
    }
}

static void init_device_info(GtkBuilder *builder, StreamDeck *deck) {
    GtkLabel *label = GTK_LABEL(gtk_builder_get_object(builder, "device_info"));
    GString *serial = stream_deck_get_serial_number(deck);
    GString *text = g_string_new(NULL);

    g_string_printf(text, "StreamDeck (%s)", serial->str);

    gtk_label_set_text(label, g_string_free(text, FALSE));
}

// TODO: Use device serial number tu read config of this device
void read_config(StreamDeck *deck) {
    const gchar *config_dir = g_get_user_config_dir();
    g_autoptr(GKeyFile) keys = g_key_file_new();
    GString *serial_number = stream_deck_get_serial_number(deck);

    g_autofree char *filename;
    filename = g_strdup_printf("%s/keys_%s.ini", config_dir, serial_number->str);

    printf("Loading from ... %s\n", filename);

    if (g_key_file_load_from_file(keys, filename, 0, NULL)) {
        gsize ngroups;
        gchar **groups = g_key_file_get_groups(keys, &ngroups);

        for (int i = 0; i < ngroups; i++) {
            printf("Loading key config %s\n", groups[i]);
            gchar **tokens = g_strsplit(groups[i], "_", 2);
            int button_index = atoi(tokens[1]);

            printf("  Got button index %d\n", button_index);

            BUTTON_ACTION[button_index] = deck_plugin_load(keys, groups[i]);

            g_strfreev(tokens);
        }

        g_strfreev(groups);
    }
}

void save_config(StreamDeck *deck) {
    const gchar *config_dir = g_get_user_config_dir();
    g_autoptr(GKeyFile) keys = g_key_file_new();
    GString *serial_number = stream_deck_get_serial_number(deck);

    g_autofree char *filename;
    filename = g_strdup_printf("%s/keys_%s.ini", config_dir, serial_number->str);

    printf("Saving ... %s\n", filename);

    for (int i = 0; i < 15; i++) {
        if (BUTTON_ACTION[i] != NULL) {
            deck_plugin_save(BUTTON_ACTION[i], i, keys);
        }
    }

    // TODO: Handle errors
    g_key_file_save_to_file(keys, filename, NULL);

    g_string_free(serial_number, TRUE);
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder;
    GtkWidget *window;
    GtkTreeView *plugin_tree;
    GtkGrid *button_grid;
    GObject *o;
    GList *devices = (GList *)user_data;

    StreamDeck *device = STREAM_DECK(devices->data);
    read_config(device);

    // Load and init plugins
    plugin_list = deck_plugin_list();

    // builder = gtk_builder_new_from_file("main.glade");
    builder = gtk_builder_new_from_resource("/ar/com/p39/gtkdeck/main.glade");

    init_device_info(builder, device);

    o = gtk_builder_get_object(builder, "main");
    window = GTK_WIDGET(o);
    gtk_window_set_title(GTK_WINDOW(window), "Gtk Deck");

    config_row = GTK_BOX(gtk_builder_get_object(builder, "config_row"));

    plugin_tree = GTK_TREE_VIEW(gtk_builder_get_object(builder, "plugin_list"));
    init_plugin_tree(plugin_tree);

    button_grid = GTK_GRID(gtk_builder_get_object(builder, "button_grid"));
    init_button_grid(button_grid, device);

    g_object_unref(builder);

    gtk_application_add_window(app, GTK_WINDOW(window));

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    GList *devices;
    int status;

    devices = stream_deck_list();

    printf("Found %d devices\n", g_list_length(devices));

    stream_deck_info(devices->data);
    stream_deck_reset_to_logo(devices->data);

    app = gtk_application_new("ar.com.p39.gtkdeck", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), devices);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    stream_deck_free(devices);
    deck_plugin_exit();

    g_object_unref(app);

    return status;
}