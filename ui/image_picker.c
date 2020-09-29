#include "image_picker.h"

static void image_picker_pick_image(ImagePicker *self, GtkButton *button);

typedef struct _ImagePickerPrivate {
    /* This is the entry defined in the GtkBuilder xml */
    GtkImage *plugin_preview;
    GtkWidget *pick_image_button;
    GtkWidget *state_row;
    GtkWidget *state_normal_button;
    GtkWidget *state_active_button;

    DeckPlugin *plugin;
} ImagePickerPrivate;

enum { PROP_0, PROP_PLUGIN };

G_DEFINE_TYPE_WITH_CODE(ImagePicker, image_picker, GTK_TYPE_BOX, G_ADD_PRIVATE(ImagePicker))

static int image_picker_signals[1];

static void image_picker_set_property(GObject *object, guint prop_id, const GValue *value,
                                      GParamSpec *pspec) {
    ImagePicker *widget = IMAGE_PICKER(object);
    ImagePickerPrivate *priv = image_picker_get_instance_private(widget);

    switch (prop_id) {
    case PROP_PLUGIN:
        priv->plugin = g_value_get_pointer(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void image_picker_get_property(GObject *object, guint prop_id, GValue *value,
                                      GParamSpec *pspec) {
    // ImagePicker *widget = IMAGE_PICKER(object);

    switch (prop_id) {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void image_picker_constructed(GObject *object) {
    ImagePicker *widget = IMAGE_PICKER(object);
    ImagePickerPrivate *priv = image_picker_get_instance_private(widget);

    if (deck_plugin_get_button_mode(priv->plugin) == BUTTON_MODE_NORMAL) {
        gtk_widget_destroy(priv->state_row);
    }

    cairo_surface_t *surface = deck_plugin_get_image(priv->plugin, BUTTON_STATE_NORMAL);
    gtk_image_set_from_surface(GTK_IMAGE(priv->plugin_preview), surface);
    printf("-> %p\n", priv);
}

static void image_picker_finalize(GObject *object) {
    // ImagePicker *widget = IMAGE_PICKER(object);
}

static void image_picker_class_init(ImagePickerClass *klass) {
    GType types[1] = {G_TYPE_POINTER};
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    widget_class = GTK_WIDGET_CLASS(klass);

    gobject_class->set_property = image_picker_set_property;
    gobject_class->get_property = image_picker_get_property;
    gobject_class->constructed = image_picker_constructed;
    gobject_class->finalize = image_picker_finalize;

    image_picker_signals[0] =
        g_signal_newv("button_pressed", G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                      NULL /* closure */, NULL /* accumulator */, NULL /* accumulator data */,
                      g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 1, types);

    /* Install a property, this is actually just a proxy for the internal GtkEntry text */
    g_object_class_install_property(
        gobject_class, PROP_PLUGIN,
        g_param_spec_pointer("plugin", "Plugin", "Plugin",
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /* Setup the template GtkBuilder xml for this class
     */
    gtk_widget_class_set_template_from_resource(widget_class,
                                                "/ar/com/p39/gtkdeck/image_picker.ui");

    /* Define the relationship of the private entry and the entry defined in the xml
     */
    gtk_widget_class_bind_template_child_private(widget_class, ImagePicker, plugin_preview);
    gtk_widget_class_bind_template_child_private(widget_class, ImagePicker, pick_image_button);
    gtk_widget_class_bind_template_child_private(widget_class, ImagePicker, state_normal_button);
    gtk_widget_class_bind_template_child_private(widget_class, ImagePicker, state_active_button);
    gtk_widget_class_bind_template_child_private(widget_class, ImagePicker, state_row);

    /* Declare the callback ports that this widget class exposes, to bind with <signal>
     * connections defined in the GtkBuilder xml
     */
    gtk_widget_class_bind_template_callback(widget_class, image_picker_pick_image);

    // g_object_set_data(G_OBJECT(state_normal_button), "image_state",
    //                   GINT_TO_POINTER(BUTTON_STATE_NORMAL));
    // g_object_set_data(G_OBJECT(state_normal_button), "plugin", plugin);
    // g_signal_connect(G_OBJECT(state_normal_button), "toggled", G_CALLBACK(image_preview_change),
    //                  plugin_preview);
    // GtkWidget *state_active_button = GTK_WIDGET(gtk_builder_get_object(builder, "state_active"));
    // g_object_set_data(G_OBJECT(state_active_button), "image_state",
    //                   GINT_TO_POINTER(BUTTON_STATE_NORMAL));
    // g_object_set_data(G_OBJECT(state_active_button), "plugin", plugin);
    // g_signal_connect(G_OBJECT(state_active_button), "toggled", G_CALLBACK(image_preview_change),
    //                  plugin_preview);
}

static void image_picker_init(ImagePicker *widget) {
    // ImagePickerPrivate *priv = image_picker_get_instance_private(widget);

    gtk_widget_init_template(GTK_WIDGET(widget));
}

/***********************************************************
 *                       Callbacks                         *
 ***********************************************************/

static void image_picker_pick_image(ImagePicker *self, GtkButton *button) {
    ImagePickerPrivate *priv = image_picker_get_instance_private(self);
    printf("2> %p\n", priv);

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

        printf("gf %p\n", priv->plugin);
        deck_plugin_set_preview_from_file(priv->plugin, filename);

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// static image_preview_change(GtkRadioButton *button, gpointer user_data) {
//     if (gtk_toggle_button_get_active(button)) {
//         DeckPlugin *plugin = g_object_get_data(G_OBJECT(button), "plugin");
//         GtkImage *image = GTK_IMAGE(user_data);
//         int state_mode = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(button), "image_state"));
//         cairo_surface_t *surface = deck_plugin_get_image(plugin, state_mode);

//         gtk_image_set_from_surface(image, surface);
//     }
// }

/***********************************************************
 *                            API                          *
 ***********************************************************/
GtkWidget *image_picker_new(DeckPlugin *plugin) {
    return g_object_new(IMAGE_TYPE_PICKER, "plugin", plugin, NULL);
}
