#include "streamdeck.h"
#include <gtk/gtk.h>

static void activate(GtkApplication *app, gpointer user_data) {
    GtkBuilder *builder;
    GtkWidget *window;
    GObject *o;

    builder = gtk_builder_new_from_file("main.glade");

    o = gtk_builder_get_object(builder, "main");
    window = GTK_WIDGET(o);
    printf("%p\n", o);
    gtk_window_set_title(GTK_WINDOW(window), "Gtk Deck");

    g_object_unref(builder);

    gtk_application_add_window(app, GTK_WINDOW(window));

    gtk_widget_show_all(window);
}
#include <unistd.h>
int main(int argc, char **argv) {
    GtkApplication *app;
    GtkBuilder *builder;
    GList *devices;
    int status;

    devices = stream_deck_list();

    printf("Found %d devices\n", g_list_length(devices));

    stream_deck_info(devices->data);
    printf("%s\n", stream_deck_get_firmware_version(devices->data)->str);
    printf("%s\n", stream_deck_get_serial_number(devices->data)->str);

    stream_deck_reset_to_logo(devices->data);
    for (int b = 1; b < 15; b++) {
        stream_deck_fill_color(devices->data, b, rand() % 255, rand() % 255, rand() % 255);
    }
    stream_deck_set_image(devices->data, 0, "example.png");

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
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    return status;
}