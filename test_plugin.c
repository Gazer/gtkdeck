#include "test_plugin.h"

void test_config_widget(DeckPlugin *plugin, GtkBox *parent);
DeckPlugin *test_clone(DeckPlugin *plugin);
void set_surface(TestPlugin *self);
void test_exec(DeckPlugin *plugin);

typedef struct _TestPluginPrivate {
    GdkRGBA color;
} TestPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(TestPlugin, test_plugin, DECK_TYPE_PLUGIN)

typedef enum { COLORED_BUTTON = 0, RED_BUTTON, N_ACTIONS } TestActionCodes;

DeckPluginInfo TEST_PLUGIN_INFO = {
    "Test Plugin",
    N_ACTIONS,
    {
        {"Colored Button", COLORED_BUTTON},
        {"Always Red", RED_BUTTON},
    },
};

static void test_plugin_init(TestPlugin *self) {
    TestPluginPrivate *priv = test_plugin_get_instance_private(self);

    priv->color.red = 0;
    priv->color.green = 0;
    priv->color.blue = 0;
    set_surface(self);
}

static void test_plugin_finalize(GObject *object) {
    TestPlugin *self = TEST_PLUGIN(object);
    TestPluginPrivate *priv = test_plugin_get_instance_private(self);

    G_OBJECT_CLASS(test_plugin_parent_class)->finalize(object);
}

DeckPluginInfo *test_info(DeckPlugin *self) { return &TEST_PLUGIN_INFO; }

static void test_plugin_class_init(TestPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    /* implement pure virtual function. */
    deck_plugin_klass->info = test_info;
    deck_plugin_klass->clone = test_clone;
    deck_plugin_klass->config_widget = test_config_widget;
    deck_plugin_klass->exec = test_exec;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = test_plugin_finalize;
}

DeckPlugin *test_plugin_new() { return g_object_new(TEST_TYPE_PLUGIN, NULL); }

DeckPlugin *test_clone(DeckPlugin *self) { return test_plugin_new(); }

void on_color_selected(GtkColorButton *widget, gpointer user_data) {
    TestPlugin *self = TEST_PLUGIN(user_data);
    TestPluginPrivate *priv = test_plugin_get_instance_private(TEST_PLUGIN(self));

    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(widget), &(priv->color));
}

void set_surface(TestPlugin *self) {
    TestPluginPrivate *priv = test_plugin_get_instance_private(TEST_PLUGIN(self));
    cairo_surface_t *preview_surface;
    cairo_t *cr;

    preview_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 72, 72);
    cr = cairo_create(preview_surface);
    cairo_set_source_rgb(cr, priv->color.red, priv->color.green, priv->color.blue);
    cairo_rectangle(cr, 0, 2, 72, 72);
    cairo_fill(cr);
    cairo_destroy(cr);

    g_object_set(G_OBJECT(self), "preview", preview_surface, NULL);
}

void test_exec(DeckPlugin *self) {
    TestPluginPrivate *priv = test_plugin_get_instance_private(TEST_PLUGIN(self));
    cairo_surface_t *preview_surface;
    cairo_t *cr;

    preview_surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24, 72, 72);
    cr = cairo_create(preview_surface);
    cairo_set_source_rgb(cr, priv->color.red, priv->color.green, priv->color.blue);
    cairo_rectangle(cr, 0, 2, 72, 72);
    cairo_fill(cr);
    cairo_destroy(cr);

    g_object_set(G_OBJECT(self), "preview", preview_surface, NULL);

    g_usleep(500000);
}

void test_config_widget(DeckPlugin *self, GtkBox *parent) {
    GList *iter = NULL;
    TestPluginPrivate *priv = test_plugin_get_instance_private(TEST_PLUGIN(self));

    GtkWidget *label = gtk_label_new("Color");
    GtkWidget *button = gtk_color_button_new();

    gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(button), &(priv->color));

    g_signal_connect(G_OBJECT(button), "color-set", G_CALLBACK(on_color_selected), self);

    gtk_box_pack_start(parent, label, TRUE, FALSE, 5);
    gtk_box_pack_start(parent, button, TRUE, FALSE, 5);
}