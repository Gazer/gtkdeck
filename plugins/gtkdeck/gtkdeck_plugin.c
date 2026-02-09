#include "gtkdeck_plugin.h"
#include "countdown.h"

DeckPlugin *gtkdeck_plugin_clone(DeckPlugin *self, int action);
DeckPlugin *gtkdeck_plugin_clone_with_code(DeckPlugin *self, int code);
DeckPluginInfo *gtkdeck_plugin_info(DeckPlugin *self);

struct _GtkDeckPlugin {
    DeckPlugin parent_instance;
};

G_DEFINE_TYPE(GtkDeckPlugin, gtkdeck_plugin, DECK_TYPE_PLUGIN)

typedef enum { COUNTDOWN = 0, N_ACTIONS } GtkDeckActionCodes;

DeckPluginInfo GTKDECK_PLUGIN_INFO = {
    "GtkDeck",
    N_ACTIONS,
    {
        {BUTTON_MODE_NORMAL, "Countdown", "/ar/com/p39/gtkdeck/plugins/gtkdeck-countdown.png", NULL,
         COUNTDOWN, countdown_config, countdown_exec, countdown_save, countdown_load},
    },
};

static void gtkdeck_plugin_init(GtkDeckPlugin *self) {
}

static void gtkdeck_plugin_finalize(GObject *object) {
    G_OBJECT_CLASS(gtkdeck_plugin_parent_class)->finalize(object);
}

static void gtkdeck_plugin_class_init(GtkDeckPluginClass *klass) {
    DeckPluginClass *deck_plugin_klass = DECK_PLUGIN_CLASS(klass);

    deck_plugin_klass->info = gtkdeck_plugin_info;
    deck_plugin_klass->clone = gtkdeck_plugin_clone;
    deck_plugin_klass->clone_with_code = gtkdeck_plugin_clone_with_code;

    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = gtkdeck_plugin_finalize;
}

DeckPlugin *gtkdeck_plugin_new() {
    return g_object_new(GTKDECK_TYPE_PLUGIN, "name", "GtkDeckPlugin", NULL);
}

DeckPlugin *gtkdeck_plugin_clone(DeckPlugin *self, int action) {
    return g_object_new(GTKDECK_TYPE_PLUGIN, "name", "GtkDeckPlugin", "action",
                        &GTKDECK_PLUGIN_INFO.actions[action], NULL);
}

DeckPluginInfo *gtkdeck_plugin_info(DeckPlugin *self) { return &GTKDECK_PLUGIN_INFO; }

DeckPlugin *gtkdeck_plugin_clone_with_code(DeckPlugin *self, int code) {
    for (int i = 0; i < N_ACTIONS; i++) {
        if (code == GTKDECK_PLUGIN_INFO.actions[i].code) {
            return gtkdeck_plugin_clone(self, i);
        }
    }
    return NULL;
}
