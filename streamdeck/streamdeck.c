#include "streamdeck.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <hidapi.h>

#define LTR 0
#define RTL 1

#define STREAM_DECK_ORIGINAL 1
#define STREAM_DECK_MINI 2
#define STREAM_DECK_XL 3
#define STREAM_DECK_ORIGINAL_V2 4

void stream_deck_init_original_v2(StreamDeck *deck);
void stream_deck_init_mini(StreamDeck *deck);
static GString *read_feature_string(hid_device *handle, int code, int size, int read_offset);

typedef struct _StreamDeckPrivate {
    hid_device *handle;
    GThread *read_thread;

    // properties
    int rows;
    int columns;
    int icon_size;
    int key_direction;
    int key_data_offset;
    int key_flip_h;
    int key_flip_v;
    GdkPixbufRotation key_rotation;
    int key_read_header;
    char *key_image_format;
    int max_packet_size;
    int packet_header_size;

    GBytes *(*reset_command)(StreamDeck *self);
    GBytes *(*brightness_command)(StreamDeck *self, int percentage);
    GString *(*get_serial_number)(StreamDeck *self);
    GString *(*get_firmware_version)(StreamDeck *self);
    void (*write_image_header)(unsigned char *packet, int key, int this_length, int bytes_remaining,
                               int page_number);

    char *key_state;

    // calculated
    int num_keys;
    int icon_bytes;
} StreamDeckPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(StreamDeck, stream_deck, G_TYPE_OBJECT)

typedef enum { DEVICE = 1, TYPE, KEY_STATES, ROWS, COLUMNS, N_PROPERTIES } StreamDeckProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

static int signals[1];

#include "stream_deck_init_original.c"
#include "stream_deck_init_original_v2.c"
#include "stream_deck_init_mini.c"
#include "stream_deck_init_xl.c"

static void stream_deck_set_property(GObject *object, guint property_id, const GValue *value,
                                     GParamSpec *pspec) {
    StreamDeck *self = STREAM_DECK(object);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    switch ((StreamDeckProperty)property_id) {
    case DEVICE:
        priv->handle = g_value_peek_pointer(value);
        break;
    case KEY_STATES: {
        char *new_key_state = g_value_peek_pointer(value);
        memcpy(priv->key_state, new_key_state, priv->num_keys);
        // g_signal_emit_by_name(object, "key_state::0", NULL);
        break;
    }
    case TYPE: {
        int type = g_value_get_int(value);
        if (type == STREAM_DECK_ORIGINAL_V2) {
            stream_deck_init_original_v2(self);
        } else if (type == STREAM_DECK_MINI) {
            stream_deck_init_mini(self);
        } else if (type == STREAM_DECK_ORIGINAL) {
            stream_deck_init_original(self);
        } else if (type == STREAM_DECK_XL) {
            stream_deck_init_xl(self);
        }
        break;
    }
    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void stream_deck_get_property(GObject *object, guint property_id, GValue *value,
                                     GParamSpec *pspec) {
    StreamDeck *self = STREAM_DECK(object);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    switch ((StreamDeckProperty)property_id) {
    case DEVICE:
        // g_value_set(value, priv->device);
        // priv->device = value;
        break;
    case ROWS: {
        g_value_set_int(value, priv->rows);
        break;
    }
    case COLUMNS: {
        g_value_set_int(value, priv->columns);
        break;
    }

    default:
        /* We don't have any other property... */
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}

static void stream_deck_init(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    printf("stream_deck_init\n");
    priv->rows = 0;
    priv->columns = 0;
    priv->icon_size = 0;
    priv->key_direction = 0;
    priv->key_data_offset = 0;
}

gpointer read_key_states(gpointer data) {
    StreamDeck *deck = STREAM_DECK(data);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(deck);
    unsigned char key_state[priv->key_read_header + priv->num_keys];

    while (TRUE) {
        memset(key_state, 0, sizeof(key_state));
        hid_read(priv->handle, key_state, sizeof(key_state));

        for (int i = 0; i < 15; i++) {
            if (priv->key_state[i] != key_state[priv->key_read_header + i]) {
                if (key_state[priv->key_read_header + i] == 1) {
                    printf("%d pressed\n", i);
                    g_signal_emit(G_OBJECT(deck), signals[0], 0, i);
                } else {
                    printf("%d released\n", i);
                }
            }
        }

        memcpy(priv->key_state, key_state + priv->key_read_header, priv->num_keys);

        usleep(5000);
        g_thread_yield();
    }
}

// https://github.com/libusb/hidapi/blob/ca1a2d6efae8d372587f4c13f60632916681d408/libusb/hid.c
static void stream_deck_constructed(GObject *object) {
    StreamDeck *deck = STREAM_DECK(object);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(deck);

    priv->num_keys = priv->rows * priv->columns;
    priv->icon_bytes = priv->icon_size * priv->icon_size * 3;
    priv->key_state = g_new0(char, priv->num_keys);

    priv->read_thread = g_thread_new("deck", read_key_states, object);

    G_OBJECT_CLASS(stream_deck_parent_class)->constructed(object);
}

static void stream_deck_finalize(GObject *obj) {
    StreamDeck *self = STREAM_DECK(obj);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    printf("stream_deck_finalize\n");

    g_free(priv->key_state);
    g_thread_unref(priv->read_thread);
    hid_close(priv->handle);

    /* Always chain up to the parent finalize function to complete object
     * destruction. */
    G_OBJECT_CLASS(stream_deck_parent_class)->finalize(obj);
}

static void stream_deck_class_init(StreamDeckClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    GType types[1] = {G_TYPE_INT};

    object_class->set_property = stream_deck_set_property;
    object_class->get_property = stream_deck_get_property;
    object_class->constructed = stream_deck_constructed;
    object_class->finalize = stream_deck_finalize;

    signals[0] =
        g_signal_newv("key_pressed", G_TYPE_FROM_CLASS(object_class),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                      NULL /* closure */, NULL /* accumulator */, NULL /* accumulator data */,
                      g_cclosure_marshal_VOID__INT, G_TYPE_NONE, 1, types);

    obj_properties[DEVICE] =
        g_param_spec_pointer("device", "Device", "USB Device where the deck is connected.",
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[KEY_STATES] = g_param_spec_pointer(
        "key_states", "Key State", "Array of state of each key", G_PARAM_READWRITE);

    obj_properties[TYPE] = g_param_spec_int("type", "Device type", "Device type.", 1, 4, 1,
                                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[ROWS] =
        g_param_spec_int("rows", "Rows", "Rows of the device.", 0, 100, 0, G_PARAM_READABLE);

    obj_properties[COLUMNS] = g_param_spec_int("columns", "Columns", "Columns of the device.", 0,
                                               100, 0, G_PARAM_READABLE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

void stream_deck_exit() { hid_exit(); }

GList *stream_deck_list() {
    int res;
    hid_device *handle;
    GList *device_list = NULL;
    StreamDeck *deck;
    struct hid_device_info *info, *iter;

    res = hid_init();
    if (res < 0) {
        printf("Can not initialize LIBUSB");
        return NULL;
    }

    info = hid_enumerate(0x0fd9, 0);
    iter = info;
    while (iter != NULL) {
        int type = -1;
        handle = hid_open(iter->vendor_id, iter->product_id, NULL);

        switch (iter->product_id) {
        case 0x0060:
            type = STREAM_DECK_ORIGINAL;
            break;
        case 0x006d:
            type = STREAM_DECK_ORIGINAL_V2;
            break;
        case 0x0063:
            type = STREAM_DECK_MINI;
            break;
        case 0x006c:
            type = STREAM_DECK_XL;
            break;
        default:
            printf("Device not yet supported\n");
        }
        if (type != -1) {
            deck = g_object_new(STREAM_TYPE_DECK, "device", handle, "type", type, NULL);

            device_list = g_list_append(device_list, deck);
        }
        iter = iter->next;
    }

    hid_free_enumeration(info);

    return device_list;
}

void stream_deck_free(GList *devices) {
    GList *iter = devices;
    g_object_unref(iter->data);
    g_list_free(devices);
}

void stream_deck_info(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    wchar_t wstr[255];

    printf("Found StreamDeck device\n");
    hid_get_manufacturer_string(priv->handle, wstr, 255);
    wprintf(L"Manufacturer String: %s\n", wstr);

    // Read the Product String
    hid_get_product_string(priv->handle, wstr, 255);
    wprintf(L"Product String: %s\n", wstr);

    // Read the Serial Number String
    hid_get_serial_number_string(priv->handle, wstr, 255);
    wprintf(L"Serial Number String: (%d) %s\n", wstr[0], wstr);

    // Read Indexed String 1
    hid_get_indexed_string(priv->handle, 1, wstr, 255);
    wprintf(L"Indexed String 1: %s\n", wstr);
}

void stream_deck_reset_to_logo(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    GBytes *bytes = priv->reset_command(self);
    gsize size;
    const unsigned char *buffer = g_bytes_get_data(bytes, &size);

    hid_send_feature_report(priv->handle, buffer, size);

    g_bytes_unref(bytes);
}

void stream_deck_set_brightness(StreamDeck *self, int percentage) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    GBytes *bytes = priv->brightness_command(self, percentage);
    gsize size;
    const unsigned char *buffer = g_bytes_get_data(bytes, &size);

    hid_send_feature_report(priv->handle, buffer, size);

    g_bytes_unref(bytes);
}

GString *stream_deck_get_firmware_version(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    return priv->get_firmware_version(self);
}

GString *stream_deck_get_serial_number(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    return priv->get_serial_number(self);
}

static void write_image(StreamDeckPrivate *priv, int key, GBytes *image_bytes) {
    const unsigned char *data;
    gsize size;
    data = g_bytes_get_data(image_bytes, &size);

    int page_number = 0;
    int bytes_remaining = (int)size;
    unsigned char packet[priv->max_packet_size];

    int IMAGE_REPORT_PAYLOAD_LENGTH = priv->max_packet_size - priv->packet_header_size;
    while (bytes_remaining > 0) {
        int this_length = bytes_remaining < IMAGE_REPORT_PAYLOAD_LENGTH
                              ? bytes_remaining
                              : IMAGE_REPORT_PAYLOAD_LENGTH;
        int bytes_sent = page_number * IMAGE_REPORT_PAYLOAD_LENGTH;

        memset(packet, 0, priv->max_packet_size);

        priv->write_image_header(packet, key, this_length, bytes_remaining, page_number);

        memcpy(packet + priv->packet_header_size, data + bytes_sent, this_length);

        hid_write(priv->handle, packet, priv->max_packet_size);

        bytes_remaining = bytes_remaining - this_length;
        page_number = page_number + 1;
    }
}

static void generic_set_image(StreamDeck *self, int key, GdkPixbuf *original) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    GdkPixbuf *scaled;
    gchar *buffer;
    gsize size;
    GError *error = NULL;

    if (gdk_pixbuf_get_width(original) != priv->icon_size ||
        gdk_pixbuf_get_height(original) != priv->icon_size) {
        scaled =
            gdk_pixbuf_scale_simple(original, priv->icon_size, priv->icon_size, GDK_INTERP_HYPER);
    } else {
        scaled = original;
    }

    if (priv->key_flip_h) {
        scaled = gdk_pixbuf_flip(scaled, FALSE);
    }
    if (priv->key_flip_v) {
        scaled = gdk_pixbuf_flip(scaled, TRUE);
    }
    if (priv->key_rotation != GDK_PIXBUF_ROTATE_NONE) {
        scaled = gdk_pixbuf_rotate_simple(scaled, priv->key_rotation);
    }

    gdk_pixbuf_save_to_buffer(scaled, &buffer, &size, priv->key_image_format, &error, "quality",
                              "100", NULL);

    GBytes *image_bytes = g_bytes_new(buffer, size);

    write_image(priv, key, image_bytes);
}

void stream_deck_set_image_from_file(StreamDeck *self, int key, gchar *file) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    GdkPixbuf *original =
        gdk_pixbuf_new_from_file_at_size(file, priv->icon_size, priv->icon_size, NULL);

    generic_set_image(self, key, original);
}

void stream_deck_set_image_from_surface(StreamDeck *self, int key, cairo_surface_t *surface) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    if (surface != NULL) {
        GdkPixbuf *original =
            gdk_pixbuf_get_from_surface(surface, 0, 0, cairo_image_surface_get_width(surface),
                                        cairo_image_surface_get_height(surface));

        generic_set_image(self, key, original);
    }
}

// Private Initializers

static GString *read_feature_string(hid_device *handle, int code, int size, int read_offset) {
    GString *value = g_string_new(NULL);
    unsigned char buf[size];
    buf[0] = code;

    hid_get_feature_report(handle, buf, sizeof(buf));
    g_string_assign(value, (const char *)(buf + read_offset));

    return value;
}
