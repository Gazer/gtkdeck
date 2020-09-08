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
    int key_rotation;
    int key_read_header;
    char *key_image_format;
    int max_packet_size;
    int packet_header_size;
    GBytes *reset_command;

    char *key_state;

    // calculated
    int num_keys;
    int icon_bytes;
} StreamDeckPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(StreamDeck, stream_deck, G_TYPE_OBJECT)

typedef enum { DEVICE = 1, TYPE, KEY_STATES, N_PROPERTIES } StreamDeckProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

static int signals[1];

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
        memcpy(priv->key_state, new_key_state, sizeof(priv->key_state));
        // g_signal_emit_by_name(object, "key_state::0", NULL);
        break;
    }
    case TYPE: {
        int type = g_value_get_int(value);
        if (type == STREAM_DECK_ORIGINAL_V2) {
            stream_deck_init_original_v2(self);
        } else if (type == STREAM_DECK_MINI) {
            stream_deck_init_mini(self);
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
        int n = hid_read(priv->handle, key_state, sizeof(key_state));

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

        memcpy(priv->key_state, key_state + 4, sizeof(priv->key_state));

        // g_object_set(G_OBJECT(deck), "key_states", key_state + 4, NULL);

        // printf("Status:\n");
        // for (int i = 0; i < 4; i++) {
        //     printf("  %d=%d\n", i, key_state[i]);
        // }
        // printf("Keys: ");
        // for (int i = 0; i < 15; i++) {
        //     printf("%d |", key_state[4 + i]);
        // }
        // printf("\n");

        usleep(5000);
        g_thread_yield();
    }
}

// https://github.com/libusb/hidapi/blob/ca1a2d6efae8d372587f4c13f60632916681d408/libusb/hid.c
static void stream_deck_constructed(GObject *object) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(STREAM_DECK(object));

    // Probably here we need to know the model/generation
    priv->num_keys = priv->rows * priv->columns;
    priv->icon_bytes = priv->icon_size * priv->icon_size * 3;
    // priv->max_packet_size = 1024;
    // priv->packet_header_size = 8;
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

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

void stream_deck_exit() { hid_exit(); }

GList *stream_deck_list() {
    int res;
    hid_device *handle;
    GList *device_list = NULL;
    StreamDeck *deck;

    res = hid_init();
    if (res < 0) {
        printf("Can not initialize LIBUSB");
        return NULL;
    }

    handle = hid_open(0x0fd9, 0x006d, NULL);
    // hid_set_nonblocking(handle, 1);
    deck = g_object_new(STREAM_TYPE_DECK, "device", handle, "type", STREAM_DECK_ORIGINAL_V2, NULL);
    stream_deck_init_original_v2(deck);

    device_list = g_list_append(device_list, deck);
}

void stream_deck_free(GList *devices) {
    GList *iter = devices;
    g_object_unref(iter->data);
    g_list_free(devices);
}

void stream_deck_info(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    wchar_t wstr[255];
    int res;

    printf("Found StreamDeck device\n");
    res = hid_get_manufacturer_string(priv->handle, wstr, 255);
    wprintf(L"Manufacturer String: %s\n", wstr);

    // Read the Product String
    res = hid_get_product_string(priv->handle, wstr, 255);
    wprintf(L"Product String: %s\n", wstr);

    // Read the Serial Number String
    res = hid_get_serial_number_string(priv->handle, wstr, 255);
    wprintf(L"Serial Number String: (%d) %s\n", wstr[0], wstr);

    // Read Indexed String 1
    res = hid_get_indexed_string(priv->handle, 1, wstr, 255);
    wprintf(L"Indexed String 1: %s\n", wstr);
}

void stream_deck_reset_to_logo(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    gsize size;
    unsigned char *buffer = g_bytes_get_data(priv->reset_command, &size);

    hid_send_feature_report(priv->handle, buffer, size);
}

void stream_deck_set_brightness(StreamDeck *self, int percentage) {
    unsigned char resetCommandBuffer[32] = {0x03, 0x08, percentage, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00};

    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    hid_send_feature_report(priv->handle, resetCommandBuffer, 32);
}

GString *stream_deck_get_firmware_version(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    GString *value = g_string_new(NULL);
    // GEN1 - 17
    // GEN2 - 32
    unsigned char buf[32];
    // GEN1 - 4
    // GEN2 - 5
    buf[0] = 5;

    hid_get_feature_report(priv->handle, buf, sizeof(buf));
    g_string_assign(value, &buf[6]);

    return value;
}

GString *stream_deck_get_serial_number(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    GString *value = g_string_new(NULL);
    // GEN1 - 17
    // GEN2 - 32
    unsigned char buf[32];
    // GEN1 - 3
    // GEN2 - 6
    buf[0] = 6;

    hid_get_feature_report(priv->handle, buf, sizeof(buf));
    g_string_assign(value, &buf[2]);

    return value;
}

gboolean is_valid_color(int n) { return n >= 0 && n <= 255; }

GBytes *imageToByteArray(StreamDeckPrivate *priv, unsigned char *image_buffer, int source_offset,
                         int source_stride, int dest_offset, int image_size) {
    // 3 is the color mode, can be 3 or 4 if has alpha
    int color_mode = 4; // rgba
    int buffer_size = dest_offset + image_size * image_size * color_mode;
    unsigned char *byteBuffer = g_malloc0(buffer_size);

    for (int y = 0; y < image_size; y++) {
        int row_offset = dest_offset + image_size * color_mode * y;
        for (int x = 0; x < image_size; x++) {
            // const { x: x2, y: y2 } = transformCoordinates(x, y)
            // GEN2 { x: this.ICON_SIZE - x - 1, y: this.ICON_SIZE - y - 1 }
            int x2 = priv->icon_size - x - 1;
            int y2 = priv->icon_size - y - 1;
            int i = y2 * source_stride + source_offset + x2 * 3;

            unsigned char red = image_buffer[i];
            unsigned char green = image_buffer[i + 1];
            unsigned char blue = image_buffer[i + 2];

            int offset = row_offset + x * color_mode;
            if (color_mode == 3) {
                // 'bgr'
                // byteBuffer.writeUInt8(blue, offset)
                // byteBuffer.writeUInt8(green, offset + 1) byteBuffer.writeUInt8(red, offset + 2)
            } else {
                byteBuffer[offset] = red;
                byteBuffer[offset + 1] = green;
                byteBuffer[offset + 2] = blue;
                byteBuffer[offset + 3] = 255;
            }
        }
    }

    return g_bytes_new(byteBuffer, buffer_size);
}

GBytes *encode_jpeg(GBytes *data, int width, int height, int stride) {
    gchar *buffer;
    gsize size;
    GError *error = NULL;
    GdkPixbuf *pix =
        gdk_pixbuf_new_from_bytes(data, GDK_COLORSPACE_RGB, TRUE, 8, width, height, stride);

    gdk_pixbuf_save_to_buffer(pix, &buffer, &size, "jpeg", &error, NULL);

    return g_bytes_new(buffer, size);
}

GBytes *convert_fill_image(StreamDeckPrivate *priv, unsigned char *buffer, int buffer_size,
                           int offset, int stride) {
    printf("to convert = %d\n", buffer_size);
    GBytes *byte_buffer = imageToByteArray(priv, buffer, offset, stride, 0, priv->icon_size);

    return encode_jpeg(byte_buffer, priv->icon_size, priv->icon_size, priv->icon_size * 4);
}

void write_image(StreamDeckPrivate *priv, int key, GBytes *image_bytes) {
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
        packet[0] = 0x02;
        packet[1] = 0x07;
        packet[2] = key;
        packet[3] = this_length == bytes_remaining ? 1 : 0;
        packet[4] = this_length & 0xFF;
        packet[5] = this_length >> 8;
        packet[6] = page_number & 0xFF;
        packet[7] = page_number >> 8;

        memcpy(packet + priv->packet_header_size, data + bytes_sent, this_length);

        hid_write(priv->handle, packet, priv->max_packet_size);

        bytes_remaining = bytes_remaining - this_length;
        page_number = page_number + 1;
    }
}

void fill_image_range(StreamDeckPrivate *priv, int key, unsigned char *buffer, int buffer_size,
                      int offset, int stride) {
    int bytes_size;
    if (key < 0 || key >= priv->num_keys) {
        g_error("Invalid key number");
        return;
    }

    GBytes *image_bytes = convert_fill_image(priv, buffer, buffer_size, offset, stride);

    write_image(priv, key, image_bytes);
}

void stream_deck_fill_color(StreamDeck *self, int key, int r, int g, int b) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    unsigned char buf[priv->icon_bytes];
    unsigned char tmp[3] = {r, g, b};

    if (key < 0 || key >= priv->num_keys) {
        g_error("Invalid key number");
        return;
    }
    if (!is_valid_color(r)) {
        g_error("Red is out of range");
    }
    if (!is_valid_color(g)) {
        g_error("Green is out of range");
    }
    if (!is_valid_color(b)) {
        g_error("Blue is out of range");
    }

    for (int i = 0; i < priv->icon_bytes; i++) {
        buf[i] = tmp[i % 3];
    }
    // TODO: Key may need to be transformed depending of the device key direction

    fill_image_range(priv, key, buf, priv->icon_bytes, 0, priv->icon_size * 3);
}

void generic_set_image(StreamDeck *self, int key, GdkPixbuf *original) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    GdkPixbuf *scaled;
    gchar *buffer;
    gsize size;
    GError *error = NULL;

    scaled =
        gdk_pixbuf_scale_simple(original, priv->icon_size, priv->icon_size, GDK_INTERP_BILINEAR);
    if (priv->key_flip_h) {
        scaled = gdk_pixbuf_flip(scaled, FALSE);
    }
    if (priv->key_flip_v) {
        scaled = gdk_pixbuf_flip(scaled, TRUE);
    }

    gdk_pixbuf_save_to_buffer(scaled, &buffer, &size, priv->key_image_format, &error, NULL);

    GBytes *image_bytes = g_bytes_new(buffer, size);

    write_image(priv, key, image_bytes);
}

void stream_deck_set_image_from_file(StreamDeck *self, int key, gchar *file) {
    GdkPixbuf *original = gdk_pixbuf_new_from_file(file, NULL);

    generic_set_image(self, key, original);
}

void stream_deck_set_image_from_surface(StreamDeck *self, int key, cairo_surface_t *surface) {
    GdkPixbuf *original = gdk_pixbuf_get_from_surface(surface, 0, 0, 72, 72);

    generic_set_image(self, key, original);
}

// Private Initializers
unsigned char original_reset_bytes[] = {
    0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
void stream_deck_init_original_v2(StreamDeck *deck) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(deck);

    priv->rows = 3;
    priv->columns = 5;
    priv->icon_size = 72;
    priv->key_direction = LTR;
    priv->key_data_offset = 4;
    priv->key_flip_h = 1;
    priv->key_flip_v = 1;
    priv->key_rotation = 0;
    priv->key_image_format = "jpeg";
    priv->key_read_header = 4;
    priv->max_packet_size = 1024;
    priv->packet_header_size = 8;
    priv->reset_command = g_bytes_new_static(original_reset_bytes, 32);
}

unsigned char mini_reset_bytes[] = {0x0B, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void stream_deck_init_mini(StreamDeck *deck) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(deck);

    priv->rows = 2;
    priv->columns = 3;
    priv->icon_size = 80;
    priv->key_direction = LTR;
    priv->key_data_offset = 4;
    priv->key_flip_h = 0;
    priv->key_flip_v = 1;
    priv->key_rotation = 90;
    priv->key_image_format = "bmp";
    priv->key_read_header = 1;
    priv->max_packet_size = 1024;
    priv->packet_header_size = 16;
    priv->reset_command = g_bytes_new_static(mini_reset_bytes, 17);
}