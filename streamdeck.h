/* inclusion guard */

#include <gdk/gdk.h>
#include <glib-object.h>
#include <gmodule.h>
#include <libusb-1.0/libusb.h>
#include <stdio.h>

#ifndef __STREAM_DECK_H__
#define __STREAM_DECK_H__

enum StreamDeckVersion {
    Original,
    Mini,
    XL,
    OriginalV2,
};

G_BEGIN_DECLS

/*
 * Type declaration.
 */
#define STREAM_TYPE_DECK stream_deck_get_type()
GLIB_AVAILABLE_IN_2_32
G_DECLARE_FINAL_TYPE(StreamDeck, stream_deck, STREAM, DECK, GObject)

struct _StreamDeckClass {
    GObjectClass parent;
    /* class members */
    // void (*do_action_public_virtual)(MamanBar *self, guint8 i);
    // void (*do_action_public_pure_virtual)(MamanBar *self, guint8 i);

    /* Padding to allow adding up to 12 new virtual functions without
     * breaking ABI. */
    gpointer padding[12];
};

struct _StreamDeck {
    GObject parent;
};

/*
 * Method definitions.
 */
GType stream_deck_get_type();
GList *stream_deck_list();
void stream_deck_free(GList *devices);
void stream_deck_info(StreamDeck *sd);
void stream_deck_reset_to_logo(StreamDeck *sd);
void stream_deck_set_brightness(StreamDeck *sd, int percentage);
GString *stream_deck_get_firmware_version(StreamDeck *sd);
GString *stream_deck_get_serial_number(StreamDeck *sd);
void stream_deck_fill_color(StreamDeck *sd, int key, int r, int g, int b);
void stream_deck_set_image_from_file(StreamDeck *sd, int key, gchar *file);
void stream_deck_set_image_from_surface(StreamDeck *sd, int key, cairo_surface_t *surface);

G_END_DECLS

#endif /* __STREAM_DECK_H__ */