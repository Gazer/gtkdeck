#include <gtk/gtk.h>
#include "streamdeck.h"
#include "deck_plugin.h"

#ifndef __DECK_GRID_H__

#define __DECK_GRID_H__

G_BEGIN_DECLS

#define DECK_TYPE_GRID (deck_grid_get_type())
#define DECK_GRID(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), DECK_TYPE_GRID, DeckGrid))
#define DECK_GRID_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), DECK_TYPE_GRID, DeckGridClass))
#define DECK_IS_GRID(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), DECK_TYPE_GRID))
#define DECK_IS_GRID_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), DECK_TYPE_GRID))
#define DECK_GRID_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), DECK_TYPE_GRID, DeckGridClass))

typedef struct _DeckGrid DeckGrid;
typedef struct _DeckGridClass DeckGridClass;
typedef struct _DeckGridPrivate DeckGridPrivate;

static GtkTargetEntry deck_grid_entries[] = {{"text/plain", GTK_TARGET_SAME_APP, 0}};
struct DATA {
    DeckPlugin *plugin;
    int action;
};

struct _DeckGrid {
    /*< private >*/
    GtkGrid parent;

    DeckGridPrivate *priv;
};

struct _DeckGridClass {
    GtkGridClass parent_class;
};

GtkWidget *deck_grid_new(StreamDeck *deck);

void deck_grid_set_button(DeckGrid *self, int key, DeckPlugin *plugin);

DeckPlugin *deck_grid_get_button(DeckGrid *self, int key);

G_END_DECLS

#endif /* __DECK_GRID_H__ */