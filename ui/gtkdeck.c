#include "deck_plugin.h"
#include "streamdeck.h"
#include "test_plugin.h"
#include "deck_grid.h"
#include <gtk/gtk.h>
#include <unistd.h>

void save_config(DeckGrid *grid, StreamDeck *deck);

GList *plugin_list = NULL;
GtkBox *config_row;
DeckGrid *global_grid;

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

    gtk_drag_source_set(GTK_WIDGET(list), GDK_BUTTON1_MASK, deck_grid_entries, 1, GDK_ACTION_COPY);
    g_signal_connect(list, "drag-data-get", G_CALLBACK(on_drag_data_get), NULL);
    gtk_tree_view_set_model(list, GTK_TREE_MODEL(treestore));
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

void show_config(GtkButton *button, DeckPlugin *plugin, gpointer data) {
    GList *children;
    GtkWidget *config_area;

    // Show Preview at the left
    children = gtk_container_get_children(GTK_CONTAINER(config_row));
    while (children != NULL) {
        GtkWidget *preview_widget = GTK_WIDGET(children->data);
        gtk_widget_destroy(preview_widget);
        children = children->next;
    }

    // Add Image picker button
    GtkBuilder *builder = gtk_builder_new_from_resource("/ar/com/p39/gtkdeck/image_picker.ui");
    GtkWidget *image_picker = GTK_WIDGET(gtk_builder_get_object(builder, "image_picker"));

    cairo_surface_t *surface = deck_plugin_get_surface(plugin);
    GtkImage *plugin_preview = GTK_IMAGE(gtk_builder_get_object(builder, "plugin_preview"));
    gtk_image_set_from_surface(plugin_preview, surface);

    GtkWidget *pick_image_button = GTK_WIDGET(gtk_builder_get_object(builder, "pick_image_button"));
    g_signal_connect(G_OBJECT(pick_image_button), "clicked", G_CALLBACK(pick_button_image), plugin);

    gtk_box_pack_start(GTK_BOX(config_row), image_picker, FALSE, TRUE, 5);

    config_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(config_row), GTK_WIDGET(config_area), TRUE, TRUE, 5);

    // Show Plugin properties at the right
    deck_plugin_get_config_widget(DECK_PLUGIN(plugin), GTK_BOX(config_area));

    gtk_widget_show_all(GTK_WIDGET(config_row));

    if (deck_plugin_get_button_mode(plugin) == BUTTON_MODE_NORMAL) {
        GtkWidget *state_row = GTK_WIDGET(gtk_builder_get_object(builder, "state_row"));
        gtk_widget_hide(state_row);
    }
}

static void init_device_info(GtkBuilder *builder, StreamDeck *deck) {
    GtkLabel *label = GTK_LABEL(gtk_builder_get_object(builder, "device_info"));
    GString *serial = stream_deck_get_serial_number(deck);
    GString *text = g_string_new(NULL);

    g_string_printf(text, "StreamDeck (%s)", serial->str);

    gtk_label_set_text(label, g_string_free(text, FALSE));
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder;
    GtkWidget *window;
    GtkTreeView *plugin_tree;
    GObject *o;
    GList *devices = (GList *)user_data;

    StreamDeck *device = STREAM_DECK(devices->data);

    // Load and init plugins
    plugin_list = deck_plugin_list();

    builder = gtk_builder_new_from_resource("/ar/com/p39/gtkdeck/main.glade");

    init_device_info(builder, device);

    o = gtk_builder_get_object(builder, "main");
    window = GTK_WIDGET(o);
    gtk_window_set_title(GTK_WINDOW(window), "Gtk Deck");

    config_row = GTK_BOX(gtk_builder_get_object(builder, "config_row"));

    plugin_tree = GTK_TREE_VIEW(gtk_builder_get_object(builder, "plugin_list"));
    init_plugin_tree(plugin_tree);

    GtkWidget *left_panel = GTK_WIDGET(gtk_builder_get_object(builder, "left_side"));

    global_grid = deck_grid_new(device);
    g_signal_connect(global_grid, "button_pressed", G_CALLBACK(show_config), device);

    gtk_box_pack_start(GTK_BOX(left_panel), global_grid, FALSE, FALSE, 8);

    gtk_widget_show(GTK_WIDGET(global_grid));

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