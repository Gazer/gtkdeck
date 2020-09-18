#include "deck_grid.h"

// static void deck_grid_ button_clicked(DeckGrid *widget, GtkButton *button);
// static void deck_grid_entry_changed(DeckGrid *widget, GtkButton *button);
static void add_plugin_button(DeckGrid *grid, int key, StreamDeck *deck, DeckPlugin *plugin,
                              int left, int top);
static void on_grid_drag_data_received(GtkWidget *wgt, GdkDragContext *context, int x, int y,
                                       GtkSelectionData *sdata, guint info, guint time,
                                       gpointer userdata);
static void on_deck_preview_update_device(GObject *gobject, GParamSpec *pspec, gpointer user_data);
static void on_deck_preview_update_app(GObject *gobject, GParamSpec *pspec, gpointer user_data);
static void on_deck_key_pressed(GObject *gobject, int key, gpointer user_data);
static void button_pressed(GtkButton *button, gpointer data);
static void read_config(DeckGrid *grid, StreamDeck *deck);
static void save_config(DeckGrid *grid, StreamDeck *deck);

struct _DeckGridPrivate {
    /* This is the entry defined in the GtkBuilder xml */
    StreamDeck *deck;
    int rows;
    int columns;
    DeckPlugin **actions;
};

enum { PROP_0, PROP_DECK };

G_DEFINE_TYPE(DeckGrid, deck_grid, GTK_TYPE_GRID);

static int deck_grid_signals[1];

static void deck_grid_set_property(GObject *object, guint prop_id, const GValue *value,
                                   GParamSpec *pspec) {
    DeckGrid *widget = DECK_GRID(object);

    switch (prop_id) {
    case PROP_DECK:
        widget->priv->deck = g_value_get_pointer(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void deck_grid_get_property(GObject *object, guint prop_id, GValue *value,
                                   GParamSpec *pspec) {
    // DeckGrid *widget = DECK_GRID(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void deck_grid_constructed(GObject *object) {
    DeckGrid *widget = DECK_GRID(object);

    g_object_get(G_OBJECT(widget->priv->deck), "rows", &(widget->priv->rows), "columns",
                 &(widget->priv->columns), NULL);

    widget->priv->actions = g_new0(DeckPlugin *, widget->priv->rows * widget->priv->columns);
    read_config(widget, widget->priv->deck);

    g_signal_connect(G_OBJECT(widget->priv->deck), "key_pressed", G_CALLBACK(on_deck_key_pressed),
                     object);

    for (int top = 0; top < widget->priv->rows; top++) {
        for (int left = 0; left < widget->priv->columns; left++) {
            int key = left + widget->priv->columns * top;
            add_plugin_button(widget, key, widget->priv->deck, widget->priv->actions[key], left,
                              top);
        }
    }
}

static void deck_grid_finalize(GObject *object) {
    DeckGrid *widget = DECK_GRID(object);

    save_config(widget, widget->priv->deck);
}

static void deck_grid_class_init(DeckGridClass *klass) {
    GType types[1] = {G_TYPE_POINTER};
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    widget_class = GTK_WIDGET_CLASS(klass);

    gobject_class->set_property = deck_grid_set_property;
    gobject_class->get_property = deck_grid_get_property;
    gobject_class->constructed = deck_grid_constructed;
    gobject_class->finalize = deck_grid_finalize;

    deck_grid_signals[0] =
        g_signal_newv("button_pressed", G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                      NULL /* closure */, NULL /* accumulator */, NULL /* accumulator data */,
                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, types);

    /* Install a property, this is actually just a proxy for the internal GtkEntry text */
    g_object_class_install_property(
        gobject_class, PROP_DECK,
        g_param_spec_pointer("deck", "Deck", "Deck", G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /* Setup the template GtkBuilder xml for this class
     */
    gtk_widget_class_set_template_from_resource(widget_class, "/ar/com/p39/gtkdeck/deck_grid.ui");

    /* Define the relationship of the private entry and the entry defined in the xml
     */
    // gtk_widget_class_bind_child(widget_class, DeckGridPrivate, entry);

    /* Declare the callback ports that this widget class exposes, to bind with <signal>
     * connections defined in the GtkBuilder xml
     */
    // gtk_widget_class_bind_callback(widget_class, deck_grid_button_clicked);
    // gtk_widget_class_bind_callback(widget_class, deck_grid_entry_changed);

    g_type_class_add_private(gobject_class, sizeof(DeckGridPrivate));
}

static void deck_grid_init(DeckGrid *widget) {
    widget->priv = G_TYPE_INSTANCE_GET_PRIVATE(widget, DECK_TYPE_GRID, DeckGridPrivate);

    gtk_widget_init_template(GTK_WIDGET(widget));
}

/***********************************************************
 *                       Callbacks                         *
 ***********************************************************/
// static void deck_grid_button_clicked(DeckGrid *widget, GtkButton *button) {
//     g_print("The button was clicked with entry text: %s\n",
//             gtk_entry_get_text(GTK_ENTRY(widget->priv->entry)));
// }

// static void deck_grid_entry_changed(DeckGrid *widget, GtkButton *button) {
//     g_print("The entry text changed: %s\n", gtk_entry_get_text(GTK_ENTRY(widget->priv->entry)));

//     /* Notify property change */
//     g_object_notify(G_OBJECT(widget), "text");
// }

static void on_deck_preview_update_device(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    StreamDeck *deck = STREAM_DECK(user_data);
    DeckPlugin *self = DECK_PLUGIN(gobject);
    cairo_surface_t *surface = deck_plugin_get_surface(self);
    int key = GPOINTER_TO_UINT(g_object_get_data(gobject, "key"));

    stream_deck_set_image_from_surface(deck, key, surface);
}

static void on_deck_preview_update_app(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    DeckPlugin *self = DECK_PLUGIN(gobject);
    cairo_surface_t *surface = deck_plugin_get_surface(self);

    gtk_image_set_from_surface(GTK_IMAGE(user_data), surface);
}

static void button_pressed(GtkButton *button, gpointer data) {
    DeckPlugin *plugin = DECK_PLUGIN(g_object_get_data(G_OBJECT(button), "plugin"));

    g_signal_emit(G_OBJECT(data), deck_grid_signals[0], 0, plugin);
}

static void add_plugin_button(DeckGrid *grid, int key, StreamDeck *deck, DeckPlugin *plugin,
                              int left, int top) {
    GtkWidget *box = gtk_button_new();
    GtkWidget *cell = gtk_grid_get_child_at(GTK_GRID(grid), left, top);

    if (cell != NULL) {
        gtk_widget_destroy(cell);
    }

    GtkWidget *widget;
    if (plugin == NULL) {
        widget = gtk_label_new("X");
    } else {
        cairo_surface_t *surface = deck_plugin_get_surface(plugin);
        widget = gtk_image_new_from_surface(surface);
    }

    if (plugin != NULL) {
        // If plugin updates, send the new image to the device
        g_object_set_data(G_OBJECT(plugin), "key", GINT_TO_POINTER(key));

        g_signal_connect(plugin, "notify::preview", G_CALLBACK(on_deck_preview_update_device),
                         deck);
        g_signal_connect(plugin, "notify::preview", G_CALLBACK(on_deck_preview_update_app), widget);
        g_object_set_data(G_OBJECT(box), "plugin", plugin);
        g_signal_connect(box, "clicked", G_CALLBACK(button_pressed), grid);

        on_deck_preview_update_device(G_OBJECT(plugin), NULL, deck);
    }

    gtk_drag_dest_set(box, GTK_DEST_DEFAULT_ALL, deck_grid_entries, 1, GDK_ACTION_COPY);
    g_signal_connect(box, "drag-data-received", G_CALLBACK(on_grid_drag_data_received), grid);

    gtk_widget_set_size_request(widget, 72, 72);

    gtk_container_add(GTK_CONTAINER(box), widget);

    gtk_grid_attach(GTK_GRID(grid), box, left, top, 1, 1);
    gtk_widget_show_all(box);
}

void on_grid_drag_data_received(GtkWidget *wgt, GdkDragContext *context, int x, int y,
                                GtkSelectionData *sdata, guint info, guint time,
                                gpointer userdata) {
    int rows, columns;
    DeckGrid *grid = DECK_GRID(userdata);
    StreamDeck *deck = grid->priv->deck;

    rows = grid->priv->rows;
    columns = grid->priv->columns;

    struct DATA *data = NULL;
    const guchar *my_data = gtk_selection_data_get_data(sdata);
    memcpy(&data, my_data, sizeof(data));

    for (int top = 0; top < rows; top++) {
        for (int left = 0; left < columns; left++) {
            GtkWidget *cell = gtk_grid_get_child_at(GTK_GRID(grid), left, top);

            if (cell == wgt) {
                int key = left + columns * top;

                if (grid->priv->actions[key] != NULL) {
                    g_object_unref(G_OBJECT(grid->priv->actions[key]));
                }

                DeckPlugin *plugin = deck_plugin_new_with_action(data->plugin, data->action);
                grid->priv->actions[key] = plugin;

                add_plugin_button(grid, key, deck, plugin, left, top);
            }
        }
    }

    g_free(data);
}

static void on_deck_key_pressed(GObject *gobject, int key, gpointer user_data) {
    DeckGrid *widget = DECK_GRID(user_data);
    DeckPlugin *plugin = widget->priv->actions[key];

    if (plugin != NULL) {
        deck_plugin_exec(plugin);
    }
}

static void read_config(DeckGrid *grid, StreamDeck *deck) {
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

            DeckPlugin *plugin = deck_plugin_load(keys, groups[i]);
            deck_grid_set_button(grid, button_index, plugin);

            g_strfreev(tokens);
        }

        g_strfreev(groups);
    }
}

static void save_config(DeckGrid *grid, StreamDeck *deck) {
    const gchar *config_dir = g_get_user_config_dir();
    g_autoptr(GKeyFile) keys = g_key_file_new();
    GString *serial_number = stream_deck_get_serial_number(deck);

    g_autofree char *filename;
    filename = g_strdup_printf("%s/keys_%s.ini", config_dir, serial_number->str);

    printf("Saving ... %s\n", filename);

    for (int i = 0; i < 15; i++) {
        DeckPlugin *plugin = deck_grid_get_button(grid, i);
        if (plugin != NULL) {
            deck_plugin_save(plugin, i, keys);
        }
    }

    // TODO: Handle errors
    g_key_file_save_to_file(keys, filename, NULL);

    g_string_free(serial_number, TRUE);
}

/***********************************************************
 *                            API                          *
 ***********************************************************/
GtkWidget *deck_grid_new(StreamDeck *deck) {
    return g_object_new(DECK_TYPE_GRID, "deck", deck, NULL);
}

void deck_grid_set_button(DeckGrid *self, int key, DeckPlugin *plugin) {
    g_return_if_fail(DECK_IS_GRID(self));

    self->priv->actions[key] = plugin;
}

DeckPlugin *deck_grid_get_button(DeckGrid *self, int key) {
    g_return_val_if_fail(DECK_IS_GRID(self), NULL);

    return self->priv->actions[key];
}
