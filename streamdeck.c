#include "streamdeck.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <hidapi.h>

#define LTR 0
#define RTL 1

typedef struct _StreamDeckPrivate {
    hid_device *handle;

    // properties
    int rows;
    int columns;
    int icon_size;
    int key_direction;
    int key_data_offset;

    // calculated
    int num_keys;
    int icon_bytes;
    int max_packet_size;
    int packet_header_size;
} StreamDeckPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(StreamDeck, stream_deck, G_TYPE_OBJECT)

typedef enum {
    DEVICE = 1,
    ROWS,
    COLUMNS,
    ICON_SIZE,
    KEY_DIRECTION,
    KEY_DATA_OFFSET,
    N_PROPERTIES
} StreamDeckProperty;

static GParamSpec *obj_properties[N_PROPERTIES] = {
    NULL,
};

static void stream_deck_set_property(GObject *object, guint property_id, const GValue *value,
                                     GParamSpec *pspec) {
    StreamDeck *self = STREAM_DECK(object);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    switch ((StreamDeckProperty)property_id) {
    case DEVICE:
        priv->handle = g_value_peek_pointer(value);
        break;
    case ROWS:
        priv->rows = g_value_get_int(value);
        break;
    case COLUMNS:
        priv->columns = g_value_get_int(value);
        break;
    case ICON_SIZE:
        priv->icon_size = g_value_get_int(value);
        break;
    case KEY_DIRECTION:
        priv->key_direction = g_value_get_int(value);
        break;
    case KEY_DATA_OFFSET:
        priv->key_data_offset = g_value_get_int(value);
        break;

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

// https://github.com/libusb/hidapi/blob/ca1a2d6efae8d372587f4c13f60632916681d408/libusb/hid.c
static void stream_deck_constructed(GObject *object) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(STREAM_DECK(object));

    // Probably here we need to know the model/generation
    priv->num_keys = priv->rows * priv->columns;
    priv->icon_bytes = priv->icon_size * priv->icon_size * 3;
    priv->max_packet_size = 1024;
    priv->packet_header_size = 8;

    G_OBJECT_CLASS(stream_deck_parent_class)->constructed(object);
}

static void stream_deck_finalize(GObject *obj) {
    StreamDeck *self = STREAM_DECK(obj);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    printf("stream_deck_finalize\n");

    hid_close(priv->handle);

    /* Always chain up to the parent finalize function to complete object
     * destruction. */
    G_OBJECT_CLASS(stream_deck_parent_class)->finalize(obj);
}

static void stream_deck_class_init(StreamDeckClass *klass) {
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = stream_deck_set_property;
    object_class->get_property = stream_deck_get_property;
    object_class->constructed = stream_deck_constructed;
    object_class->finalize = stream_deck_finalize;

    obj_properties[DEVICE] =
        g_param_spec_pointer("device", "Device", "USB Device where the deck is connected.",
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[ROWS] = g_param_spec_int("rows", "Rows", "Rows.", 1, 10, 1,
                                            G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[COLUMNS] = g_param_spec_int("columns", "Columns", "Columns.", 1, 10, 1,
                                               G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[ICON_SIZE] = g_param_spec_int("icon_size", "Icon size", "Icon size.", 1, 100, 1,
                                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[KEY_DIRECTION] =
        g_param_spec_int("key_direction", "Key direction", "Key direction.", 0, 1, 0,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    obj_properties[KEY_DATA_OFFSET] =
        g_param_spec_int("key_data_offset", "Key data offset", "Key data offset.", 1, G_MAXINT, 1,
                         G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
}

StreamDeck *stream_deck_new(hid_device *handle, int rows, int columns, int icon_size,
                            int key_direction, int key_data_offset) {
    StreamDeck *deck = g_object_new(STREAM_TYPE_DECK, "device", handle, "rows", rows, "columns",
                                    columns, "icon_size", icon_size, "key_direction", key_direction,
                                    "key_data_offset", key_data_offset, NULL);

    return deck;
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
    deck = stream_deck_new(handle, 3, 5, 72, LTR, 4);
    device_list = g_list_append(device_list, deck);
}

void stream_deck_info(StreamDeck *sd) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);
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

void stream_deck_reset_to_logo(StreamDeck *sd) {
    // TODO: This is model specific
    unsigned char resetCommandBuffer[32] = {0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);

    hid_send_feature_report(priv->handle, resetCommandBuffer, 32);
}

void stream_deck_set_brightness(StreamDeck *sd, int percentage) {
    unsigned char resetCommandBuffer[32] = {0x03, 0x08, percentage, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00};

    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);

    hid_send_feature_report(priv->handle, resetCommandBuffer, 32);
}

GString *stream_deck_get_firmware_version(StreamDeck *sd) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);
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

GString *stream_deck_get_serial_number(StreamDeck *sd) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);
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

void stream_deck_fill_color(StreamDeck *sd, int key, int r, int g, int b) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);
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

void generic_set_image(StreamDeck *sd, int key, GdkPixbuf *original) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);
    GdkPixbuf *scaled;
    gchar *buffer;
    gsize size;
    GError *error = NULL;

    scaled = gdk_pixbuf_scale_simple(original, 72, 72, GDK_INTERP_BILINEAR);
    scaled = gdk_pixbuf_flip(scaled, FALSE);
    scaled = gdk_pixbuf_flip(scaled, TRUE);

    gdk_pixbuf_save_to_buffer(scaled, &buffer, &size, "jpeg", &error, NULL);

    GBytes *image_bytes = g_bytes_new(buffer, size);

    write_image(priv, key, image_bytes);
}

void stream_deck_set_image_from_file(StreamDeck *sd, int key, gchar *file) {
    GdkPixbuf *original = gdk_pixbuf_new_from_file(file, NULL);

    generic_set_image(sd, key, original);
}

void stream_deck_set_image_from_surface(StreamDeck *sd, int key, cairo_surface_t *surface) {
    GdkPixbuf *original = gdk_pixbuf_get_from_surface(surface, 0, 0, 72, 72);

    generic_set_image(sd, key, original);
}

void stream_deck_read_key_states(StreamDeck *sd) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);
    unsigned char key_state[4 + 15];

    memset(key_state, 0, sizeof(key_state));
    int n = hid_read(priv->handle, key_state, sizeof(key_state));

    printf("Status:\n");
    for (int i = 0; i < 4; i++) {
        printf("  %d=%d\n", i, key_state[i]);
    }
    printf("Keys:\n");
    for (int i = 0; i < 15; i++) {
        printf("  %d = %d\n", i, key_state[4 + i]);
    }
}