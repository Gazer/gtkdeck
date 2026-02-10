#include "deck_grid.h"
#include "deck_plugin.h"
#include "image_picker.h"
#include "libobsws.h"
#include "streamdeck.h"
#include "test_plugin.h"
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <unistd.h>

GList *plugin_list = NULL;
GtkBox *config_row;
GtkWidget *global_grid;
GtkWidget *main_window = NULL;
GtkWidget *delete_button;
GtkWidget *config_container;
GtkWidget *action_name_label;
DeckPlugin *current_plugin = NULL;
AppIndicator *indicator = NULL;

typedef struct {
    GList *devices;
    gboolean start_hidden;
} AppContext;

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

void show_config(GtkButton *button, DeckPlugin *plugin, gpointer data) {
    GList *children;
    GtkWidget *config_area;

    if (plugin == NULL) {
        current_plugin = NULL;
        gtk_widget_hide(config_container);
        return;
    }

    // Store the current plugin being configured
    current_plugin = plugin;

    DeckPluginInfo *info = deck_plugin_get_info(plugin);
    char *action_name = deck_plugin_get_action_name(plugin);
    g_autofree char *label_text = g_strdup_printf("%s: %s", info->name, action_name);
    gtk_label_set_text(GTK_LABEL(action_name_label), label_text);

    // Show Preview at the left
    children = gtk_container_get_children(GTK_CONTAINER(config_row));
    while (children != NULL) {
        GtkWidget *preview_widget = GTK_WIDGET(children->data);
        gtk_widget_destroy(preview_widget);
        children = children->next;
    }

    // Add Image picker button
    GtkWidget *image_picker = image_picker_new(plugin);

    gtk_box_pack_start(GTK_BOX(config_row), image_picker, FALSE, TRUE, 5);

    config_area = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(config_row), GTK_WIDGET(config_area), TRUE, TRUE, 5);

    // Show Plugin properties at the right
    deck_plugin_get_config_widget(DECK_PLUGIN(plugin), GTK_BOX(config_area));

    gtk_widget_show_all(GTK_WIDGET(config_row));
    gtk_widget_show(config_container);
}

static void init_device_info(GtkBuilder *builder, StreamDeck *deck) {
    GtkLabel *label = GTK_LABEL(gtk_builder_get_object(builder, "device_info"));
    GString *serial = stream_deck_get_serial_number(deck);
    GString *text = g_string_new(NULL);

    g_string_printf(text, "StreamDeck (%s)", serial->str);

    gtk_label_set_text(label, g_string_free(text, FALSE));
}

static void on_delete_clicked(GtkButton *button, gpointer user_data) {
    if (current_plugin == NULL) {
        return;
    }

    int key = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(current_plugin), "key"));
    deck_grid_remove_button(DECK_GRID(global_grid), key);

    // Clear the config panel
    GList *children = gtk_container_get_children(GTK_CONTAINER(config_row));
    while (children != NULL) {
        GtkWidget *child = GTK_WIDGET(children->data);
        gtk_widget_destroy(child);
        children = children->next;
    }

    current_plugin = NULL;
    gtk_widget_hide(config_container);
}

void on_menu_quit(GtkMenuItem *item, gpointer user_data) {
    GtkApplication *app = GTK_APPLICATION(user_data);
    g_application_quit(G_APPLICATION(app));
}

void on_menu_toggle(GtkMenuItem *item, gpointer user_data) {
    if (gtk_widget_get_visible(main_window)) {
        gtk_widget_hide(main_window);
    } else {
        gtk_widget_show(main_window);
        gtk_window_present(GTK_WINDOW(main_window));
    }
}

gboolean on_window_delete_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
    gtk_widget_hide(widget);
    return TRUE;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder;
    GtkTreeView *plugin_tree;
    GObject *o;
    AppContext *ctx = (AppContext *)user_data;
    GList *devices = ctx->devices;

    StreamDeck *device = STREAM_DECK(devices->data);

    // Load and init plugins
    plugin_list = deck_plugin_list();

    builder = gtk_builder_new_from_resource("/ar/com/p39/gtkdeck/main.glade");

    init_device_info(builder, device);

    o = gtk_builder_get_object(builder, "main");
    main_window = GTK_WIDGET(o);
    gtk_window_set_title(GTK_WINDOW(main_window), "Gtk Deck");
    g_signal_connect(main_window, "delete-event", G_CALLBACK(on_window_delete_event), NULL);

    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_resource("/ar/com/p39/gtkdeck/generic_icon.png", NULL);
    if (pixbuf) {
        gdk_pixbuf_save(pixbuf, "/tmp/gtkdeck-icon.png", "png", NULL, NULL);
        g_object_unref(pixbuf);
    }

    indicator = app_indicator_new("gtkdeck", "gtkdeck-icon", APP_INDICATOR_CATEGORY_APPLICATION_STATUS);
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_icon_theme_path(indicator, "/tmp");

    GtkWidget *menu = gtk_menu_new();

    GtkWidget *toggle_item = gtk_menu_item_new_with_label("Show/Hide");
    g_signal_connect(toggle_item, "activate", G_CALLBACK(on_menu_toggle), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), toggle_item);

    GtkWidget *quit_item = gtk_menu_item_new_with_label("Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(on_menu_quit), app);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

    gtk_widget_show_all(menu);
    app_indicator_set_menu(indicator, GTK_MENU(menu));

    config_row = GTK_BOX(gtk_builder_get_object(builder, "config_row"));
    config_container = GTK_WIDGET(gtk_builder_get_object(builder, "config_container"));
    action_name_label = GTK_WIDGET(gtk_builder_get_object(builder, "action_name"));

    delete_button = GTK_WIDGET(gtk_builder_get_object(builder, "delete_button"));
    g_signal_connect(delete_button, "clicked", G_CALLBACK(on_delete_clicked), NULL);

    plugin_tree = GTK_TREE_VIEW(gtk_builder_get_object(builder, "plugin_list"));
    init_plugin_tree(plugin_tree);

    GtkWidget *left_panel = GTK_WIDGET(gtk_builder_get_object(builder, "left_side"));

    global_grid = deck_grid_new(device);
    g_signal_connect(global_grid, "button_pressed", G_CALLBACK(show_config), device);

    gtk_box_pack_start(GTK_BOX(left_panel), global_grid, FALSE, FALSE, 8);

    gtk_widget_show(GTK_WIDGET(global_grid));

    g_object_unref(builder);

    gtk_application_add_window(app, GTK_WINDOW(main_window));

    gtk_widget_show_all(main_window);
    if (ctx->start_hidden) {
        gtk_widget_hide(main_window);
    }
    gtk_widget_hide(config_container);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    GList *devices;
    int status;
    gboolean start_hidden = FALSE;

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--start-hidden") == 0) {
            start_hidden = TRUE;
        }
    }

    devices = stream_deck_list();

    printf("Found %d devices\n", g_list_length(devices));

    if (g_list_length(devices) == 0) {
        return 1;
    }

    stream_deck_info(devices->data);

    // Init WS library
    // TODO: we need to do this only if the user try to use the obs plugin
    // obs_ws_new();

    AppContext ctx = {.devices = devices, .start_hidden = start_hidden};

    app = gtk_application_new("ar.com.p39.gtkdeck", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &ctx);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    if (global_grid != NULL) {
        deck_grid_save_config(DECK_GRID(global_grid));
    }

    unlink("/tmp/gtkdeck-icon.png");

    stream_deck_reset_to_logo(devices->data);
    stream_deck_free(devices);
    deck_plugin_exit();

    g_object_unref(app);

    return status;
}
