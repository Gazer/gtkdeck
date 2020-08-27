#include "streamdeck.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib-object.h>
#include <libusb-1.0/libusb.h>

#define LTR 0
#define RTL 1

typedef struct _StreamDeckPrivate {
    libusb_device *device;
    libusb_device_handle *handle;
    int output_endpoint;
    int interface;
    int nb_ifaces;

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
        priv->device = g_value_peek_pointer(value);
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
    priv->output_endpoint = 0;
}

static void stream_deck_constructed(GObject *object) {
    struct libusb_device_descriptor dev_desc;
    struct libusb_config_descriptor *conf_desc;

    StreamDeckPrivate *priv = stream_deck_get_instance_private(STREAM_DECK(object));

    // Probably here we need to know the model/generation
    priv->num_keys = priv->rows * priv->columns;
    priv->icon_bytes = priv->icon_size * priv->icon_size * 3;
    priv->max_packet_size = 1024;
    priv->packet_header_size = 8;

    libusb_get_device_descriptor(priv->device, &dev_desc);
    libusb_get_config_descriptor(priv->device, 0, &conf_desc);
    priv->nb_ifaces = conf_desc->bNumInterfaces;

    libusb_open(priv->device, &(priv->handle));
    // TODO: Handle error codes from libusb_open

    for (int j = 0; j < conf_desc->bNumInterfaces; j++) {
        const struct libusb_interface *intf = &conf_desc->interface[j];
        for (int k = 0; k < intf->num_altsetting; k++) {
            const struct libusb_interface_descriptor *intf_desc;
            intf_desc = &intf->altsetting[k];
            if (intf_desc->bInterfaceClass == LIBUSB_CLASS_HID) {
                priv->interface = intf_desc->bInterfaceNumber;

                for (int i = 0; i < intf_desc->bNumEndpoints; i++) {
                    const struct libusb_endpoint_descriptor *ep = &intf_desc->endpoint[i];

                    /* Determine the type and direction of this
                       endpoint. */
                    int is_interrupt = (ep->bmAttributes & LIBUSB_TRANSFER_TYPE_MASK) ==
                                       LIBUSB_TRANSFER_TYPE_INTERRUPT;
                    int is_output =
                        (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT;
                    int is_input =
                        (ep->bEndpointAddress & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN;

                    /* Decide whether to use it for input or output. */
                    // if (dev->input_endpoint == 0 && is_interrupt && is_input) {
                    //     /* Use this endpoint for INPUT */
                    //     dev->input_endpoint = ep->bEndpointAddress;
                    //     dev->input_ep_max_packet_size = ep->wMaxPacketSize;
                    // }
                    if (priv->output_endpoint == 0 && is_interrupt && is_output) {
                        /* Use this endpoint for OUTPUT */
                        printf("Endpoint %d\n", ep->bEndpointAddress);
                        priv->output_endpoint = ep->bEndpointAddress;
                    }
                }
            }
        }
    }

    for (int i = 0; i < priv->nb_ifaces; i++) {
        // r =
        // TODO: Errors!
        libusb_claim_interface(priv->handle, i);
        // if (r != LIBUSB_SUCCESS) {
        //     printf("   Failed.\n");
        // }
    }

    G_OBJECT_CLASS(stream_deck_parent_class)->constructed(object);
}

static void stream_deck_finalize(GObject *obj) {
    StreamDeck *self = STREAM_DECK(obj);
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);

    printf("stream_deck_finalize\n");

    for (int iface = 0; iface < priv->nb_ifaces; iface++) {
        libusb_release_interface(priv->handle, iface);
    }

    libusb_close(priv->handle);

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

StreamDeck *stream_deck_new(libusb_device *device, int rows, int columns, int icon_size,
                            int key_direction, int key_data_offset) {
    StreamDeck *deck = g_object_new(STREAM_TYPE_DECK, "device", device, "rows", rows, "columns",
                                    columns, "icon_size", icon_size, "key_direction", key_direction,
                                    "key_data_offset", key_data_offset, NULL);

    return deck;
}

GList *stream_deck_list() {
    int r;
    ssize_t size;
    struct libusb_device **list;
    GList *device_list = NULL;

    r = libusb_init(NULL);
    if (r < 0) {
        printf("Can not initialize LIBUSB");
        return NULL;
    }

    size = libusb_get_device_list(NULL, &list);

    for (int i = 0; i < size; i++) {
        struct libusb_device_descriptor dev_desc;

        libusb_get_device_descriptor(list[i], &dev_desc);

        if (dev_desc.idVendor == 0x0fd9) {
            StreamDeck *deck;

            printf("Found StreamDeck device\n");

            if (dev_desc.idProduct == 0x0060) {
                printf("             model: Original\n");
            } else if (dev_desc.idProduct == 0x0063) {
                printf("             model: Mini\n");
            } else if (dev_desc.idProduct == 0x006c) {
                printf("             model: XL\n");
            } else if (dev_desc.idProduct == 0x006d) {
                deck = stream_deck_new(list[i], 3, 5, 72, LTR, 4);
            }

            device_list = g_list_append(device_list, deck);
        }
    }

    libusb_free_device_list(list, 1);

    return device_list;
}

void stream_deck_info(StreamDeck *sd) {
    struct libusb_device_descriptor dev_desc;
    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);

    libusb_get_device_descriptor(priv->device, &dev_desc);

    if (dev_desc.idVendor == 0x0fd9) {
        printf("Found StreamDeck device\n");

        if (dev_desc.idProduct == 0x0060) {
            printf("             model: Original\n");
        } else if (dev_desc.idProduct == 0x0063) {
            printf("             model: Mini\n");
        } else if (dev_desc.idProduct == 0x006c) {
            printf("             model: XL\n");
        } else if (dev_desc.idProduct == 0x006d) {
            printf("             model: Original V2\n");
        }

        printf("            length: %d\n", dev_desc.bLength);
        printf("      device class: %d\n", dev_desc.bDeviceClass);
        printf("               S/N: %d\n", dev_desc.iSerialNumber);
        printf("           VID:PID: %04X:%04X\n", dev_desc.idVendor, dev_desc.idProduct);
        printf("         bcdDevice: %04X\n", dev_desc.bcdDevice);
        printf("   iMan:iProd:iSer: %d:%d:%d\n", dev_desc.iManufacturer, dev_desc.iProduct,
               dev_desc.iSerialNumber);
        printf("          nb confs: %d\n", dev_desc.bNumConfigurations);
    } else {
        printf("BAD \n");
    }
}

int send_feature_report(StreamDeckPrivate *priv, const unsigned char *data, size_t length) {
    int res = -1;
    int skipped_report_id = 0;
    int report_number = data[0];

    if (report_number == 0x0) {
        data++;
        length--;
        skipped_report_id = 1;
    }

    res = libusb_control_transfer(
        priv->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
        0x09 /*HID set_report*/, (3 /*HID feature*/ << 8) | report_number, priv->interface,
        (unsigned char *)data, length, 1000 /*timeout millis*/);

    if (res < 0)
        return res;

    /* Account for the report ID */
    if (skipped_report_id)
        length++;

    return length;
}

int get_feature_report(StreamDeckPrivate *priv, unsigned char *data, size_t length) {
    int res = -1;
    int skipped_report_id = 0;
    int report_number = data[0];

    if (report_number == 0x0) {
        /* Offset the return buffer by 1, so that the report ID
           will remain in byte 0. */
        data++;
        length--;
        skipped_report_id = 1;
    }
    res = libusb_control_transfer(
        priv->handle, LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_IN,
        0x01 /*HID get_report*/, (3 /*HID feature*/ << 8) | report_number, priv->interface,
        (unsigned char *)data, length, 1000 /*timeout millis*/);

    if (res < 0)
        return -1;

    if (skipped_report_id)
        res++;

    return res;
}

int usb_write(StreamDeckPrivate *priv, const unsigned char *data, size_t length) {
    int res = -1;
    int report_number = data[0];
    int skipped_report_id = 0;

    if (report_number == 0x0) {
        data++;
        length--;
        skipped_report_id = 1;
    }

    if (priv->output_endpoint <= 0) {
        /* No interrupt out endpoint. Use the Control Endpoint */
        res = libusb_control_transfer(
            priv->handle,
            LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE | LIBUSB_ENDPOINT_OUT,
            0x09 /*HID Set_Report*/, (2 /*HID output*/ << 8) | report_number, 0,
            (unsigned char *)data, length, 1000 /*timeout millis*/);
        printf("%d\n", res);

        if (res < 0)
            return -1;

        if (skipped_report_id)
            length++;

        return length;
    } else {
        /* Use the interrupt out endpoint */
        printf("Write to endpoint %d\n", priv->output_endpoint);
        int actual_length;
        res = libusb_interrupt_transfer(priv->handle, priv->output_endpoint, (unsigned char *)data,
                                        length, &actual_length, 1000);

        printf("result = %d, l=%d\n", res, actual_length);
        if (res < 0)
            return -1;

        if (skipped_report_id)
            actual_length++;

        return actual_length;
    }
}

void stream_deck_reset_to_logo(StreamDeck *sd) {
    // TODO: This is model specific
    unsigned char resetCommandBuffer[32] = {0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);

    send_feature_report(priv, resetCommandBuffer, 32);
}

void stream_deck_set_brightness(StreamDeck *sd, int percentage) {
    unsigned char resetCommandBuffer[32] = {0x03, 0x08, percentage, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x00, 0x00, 0x00,       0x00, 0x00, 0x00, 0x00, 0x00};

    StreamDeckPrivate *priv = stream_deck_get_instance_private(sd);

    send_feature_report(priv, resetCommandBuffer, 32);
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

    get_feature_report(priv, buf, 32);
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

    get_feature_report(priv, buf, 32);
    g_string_assign(value, &buf[2]);

    return value;
}

gboolean is_valid_color(int n) { return n >= 0 && n <= 255; }

GBytes *imageToByteArray(unsigned char *image_buffer, int source_offset, int source_stride,
                         int dest_offset, int image_size) {
    // 3 is the color mode, can be 3 or 4 if has alpha
    int color_mode = 4; // rgba
    int buffer_size = dest_offset + image_size * image_size * color_mode;
    unsigned char *byteBuffer = g_malloc0(buffer_size);

    for (int y = 0; y < image_size; y++) {
        int row_offset = dest_offset + image_size * color_mode * y;
        for (int x = 0; x < image_size; x++) {
            // const { x: x2, y: y2 } = transformCoordinates(x, y)
            int x2 = x;
            int y2 = y;
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
    gdk_pixbuf_save(pix, "ver.jpg", "jpeg", &error, NULL);
    return g_bytes_new(buffer, size);
}

GBytes *convert_fill_image(StreamDeckPrivate *priv, unsigned char *buffer, int buffer_size,
                           int offset, int stride) {
    GBytes *byte_buffer = imageToByteArray(buffer, offset, stride, 0, priv->icon_size);

    return encode_jpeg(byte_buffer, priv->icon_size, priv->icon_size, priv->icon_size * 4);
}

void fill_image_range(StreamDeckPrivate *priv, int key, unsigned char *buffer, int buffer_size,
                      int offset, int stride) {
    int bytes_size;
    if (key < 0 || key >= priv->num_keys) {
        g_error("Invalid key number");
        return;
    }

    GBytes *image_bytes = convert_fill_image(priv, buffer, buffer_size, offset, stride);

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

        usb_write(priv, packet, priv->max_packet_size);

        bytes_remaining = bytes_remaining - this_length;
        page_number = page_number + 1;
    }
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