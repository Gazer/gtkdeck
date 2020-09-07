#include "deck_plugin.h"
#include "streamdeck.h"
#include "test_plugin.h"
#include <gtk/gtk.h>
#include <unistd.h>

GList *plugin_list = NULL;
GtkBox *config_row;

DeckPlugin *BUTTON_ACTION[15] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

static GtkTargetEntry entries[] = {{"text/plain", GTK_TARGET_SAME_APP, 0}};

void on_drag_data_get(GtkWidget *widget, GdkDragContext *drag_context, GtkSelectionData *sdata,
                      guint info, guint time, gpointer user_data) {
    printf("Sending plugin name: color_button\n");

    gtk_selection_data_set(sdata, GDK_SELECTION_TYPE_ATOM, 32, (const guchar *)&user_data,
                           sizeof(gpointer));
}

void init_plugin_list(GtkListBox *list) {
    GList *iter = plugin_list;

    while (iter != NULL) {
        DeckPlugin *plugin = DECK_PLUGIN(iter->data);
        GtkWidget *color_button;

        color_button = gtk_button_new_with_label(deck_plugin_get_name(plugin));

        gtk_drag_source_set(color_button, GDK_BUTTON1_MASK, entries, 1, GDK_ACTION_COPY);
        g_signal_connect(color_button, "drag-data-get", G_CALLBACK(on_drag_data_get), plugin);
        gtk_list_box_insert(list, color_button, -1);

        iter = iter->next;
    }
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
    StreamDeck *deck = STREAM_DECK(data);
    GList *children, *iter;
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
    StreamDeck *deck = STREAM_DECK(gobject);
    DeckPlugin *plugin = BUTTON_ACTION[key];

    if (plugin != NULL) {
        deck_plugin_exec(plugin);
    }
}

void on_drag_data_received(GtkWidget *wgt, GdkDragContext *context, int x, int y,
                           GtkSelectionData *sdata, guint info, guint time, gpointer userdata) {
    GtkGrid *grid = GTK_GRID(userdata);
    StreamDeck *deck = STREAM_DECK(g_object_get_data(G_OBJECT(grid), "deck"));

    for (int top = 0; top < 3; top++) {
        for (int left = 0; left < 5; left++) {
            GtkWidget *cell = gtk_grid_get_child_at(grid, left, top);

            if (cell == wgt) {
                int key = left + 5 * top;

                if (BUTTON_ACTION[key] != NULL) {
                    g_object_unref(G_OBJECT(BUTTON_ACTION[key]));
                }

                GtkWidget *box = gtk_button_new();

                DeckPlugin *plugin = DECK_PLUGIN(*(gpointer *)gtk_selection_data_get_data(sdata));
                plugin = deck_plugin_clone(plugin);
                g_object_set_data(G_OBJECT(plugin), "key", GINT_TO_POINTER(key));

                gtk_widget_destroy(cell);
                BUTTON_ACTION[key] = plugin;
                GtkWidget *widget = grid_button(key);

                // If plugin updates, send the new image to the device
                g_signal_connect(plugin, "notify::preview",
                                 G_CALLBACK(on_deck_preview_update_device), deck);
                g_signal_connect(plugin, "notify::preview", G_CALLBACK(on_deck_preview_update_app),
                                 widget);

                gtk_drag_dest_set(box, GTK_DEST_DEFAULT_ALL, entries, 1, GDK_ACTION_COPY);
                g_signal_connect(box, "drag-data-received", G_CALLBACK(on_drag_data_received),
                                 grid);
                g_object_set_data(G_OBJECT(box), "plugin", plugin);
                g_signal_connect(box, "clicked", G_CALLBACK(show_config), deck);

                gtk_container_add(GTK_CONTAINER(box), widget);

                gtk_grid_attach(grid, box, left, top, 1, 1);
                gtk_widget_show_all(box);
            }
        }
    }
}

void init_button_grid(GtkGrid *grid, StreamDeck *deck) {
    g_object_set_data(G_OBJECT(grid), "deck", deck);

    g_signal_connect(deck, "key_pressed", G_CALLBACK(on_deck_key_changed), NULL);

    for (int top = 0; top < 3; top++) {
        for (int left = 0; left < 5; left++) {
            GtkWidget *widget;

            widget = grid_button(left + 5 * top);

            gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL, entries, 1, GDK_ACTION_COPY);
            // g_signal_connect(widget, "drag-drop", G_CALLBACK(on_plugin_drop), NULL);
            g_signal_connect(widget, "drag-data-received", G_CALLBACK(on_drag_data_received), grid);

            gtk_grid_attach(grid, widget, left, top, 1, 1);
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

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder;
    GtkWidget *window;
    GtkListBox *list;
    GtkGrid *button_grid;
    GObject *o;
    GList *devices = (GList *)user_data;

    StreamDeck *device = STREAM_DECK(devices->data);

    // Load and init plugins
    plugin_list = g_list_append(plugin_list, test_plugin_new());

    builder = gtk_builder_new_from_file("main.glade");

    init_device_info(builder, device);

    o = gtk_builder_get_object(builder, "main");
    window = GTK_WIDGET(o);
    gtk_window_set_title(GTK_WINDOW(window), "Gtk Deck");

    config_row = GTK_BOX(gtk_builder_get_object(builder, "config_row"));

    list = GTK_LIST_BOX(gtk_builder_get_object(builder, "plugin_list"));
    init_plugin_list(list);

    button_grid = GTK_GRID(gtk_builder_get_object(builder, "button_grid"));
    init_button_grid(button_grid, device);

    g_object_unref(builder);

    gtk_application_add_window(app, GTK_WINDOW(window));

    gtk_widget_show_all(window);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    GtkBuilder *builder;
    GList *devices;
    int status;

    devices = stream_deck_list();

    printf("Found %d devices\n", g_list_length(devices));

    stream_deck_info(devices->data);
    stream_deck_reset_to_logo(devices->data);

    // printf("%s\n", stream_deck_get_firmware_version(devices->data)->str);
    // printf("%s\n", stream_deck_get_serial_number(devices->data)->str);

    // stream_deck_reset_to_logo(devices->data);
    // for (int b = 1; b < 15; b++) {
    //     stream_deck_fill_color(devices->data, b, rand() % 255, rand() % 255, rand() % 255);
    // }
    // stream_deck_set_image(devices->data, 0, "example.png");

    // stream_deck_fill_color(devices->data, 1, 0, 255, 0);
    // stream_deck_fill_color(devices->data, 2, 0, 0, 255);

    // stream_deck_set_brightness(devices->data, 25);
    // sleep(1);
    // stream_deck_set_brightness(devices->data, 55);
    // sleep(1);
    // stream_deck_set_brightness(devices->data, 75);
    // sleep(1);
    // stream_deck_set_brightness(devices->data, 100);

    app = gtk_application_new("ar.com.p39.gtkdeck", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), devices);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    stream_deck_free(devices);

    g_object_unref(app);

    return status;
}