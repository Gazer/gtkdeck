#include "test_plugin.h"

GtkWidget *test_preview_widget(DeckPlugin *plugin);

typedef struct _TestPluginPrivate {
} TestPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(TestPlugin, test_plugin, DECK_TYPE_PLUGIN)

static void test_plugin_init(TestPlugin *self) {}

static void test_plugin_class_init(TestPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    /* implement pure virtual function. */
    deck_plugin_klass->preview_widget = test_preview_widget;
}

DeckPlugin *test_plugin_new() {
    DeckPlugin *plugin = g_object_new(TEST_TYPE_PLUGIN, "name", "Color Button", NULL);
    return plugin;
}

// Private members
GtkWidget *test_preview_widget(DeckPlugin *plugin) { return gtk_image_new_from_file("tini.png"); }