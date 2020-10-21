// XL V2 SPECIFIC FUNCTIONS

unsigned char xl_reset_data[] = {0x03, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

GBytes *xl_reset_bytes(StreamDeck *deck) { return g_bytes_new_static(xl_reset_data, 32); }

GString *xl_get_firmware_version(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    return read_feature_string(priv->handle, 0x05, 32, 6);
}

GString *xl_get_serial_number(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    return read_feature_string(priv->handle, 0x06, 32, 2);
}

GBytes *xl_brightness_command(StreamDeck *self, int percentage) {
    unsigned char *bytes = g_malloc0(sizeof(unsigned char) * 32);

    bytes[0] = 0x03;
    bytes[1] = 0x08;
    bytes[2] = percentage;

    return g_bytes_new(bytes, 32);
}

void xl_write_image_header(unsigned char *packet, int key, int this_length, int bytes_remaining,
                           int page_number) {
    packet[0] = 0x02;
    packet[1] = 0x07;
    packet[2] = key;
    packet[3] = this_length == bytes_remaining ? 1 : 0;
    packet[4] = this_length & 0xFF;
    packet[5] = this_length >> 8;
    packet[6] = page_number & 0xFF;
    packet[7] = page_number >> 8;
}

void stream_deck_init_xl(StreamDeck *deck) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(deck);

    priv->rows = 4;
    priv->columns = 8;
    priv->icon_size = 96;
    priv->key_direction = LTR;
    priv->key_data_offset = 4;
    priv->key_flip_h = 1;
    priv->key_flip_v = 1;
    priv->key_rotation = 0;
    priv->key_image_format = "jpeg";
    priv->key_read_header = 4;
    priv->max_packet_size = 1024;
    priv->packet_header_size = 8;
    priv->reset_command = xl_reset_bytes;
    priv->brightness_command = xl_brightness_command;
    priv->get_firmware_version = xl_get_firmware_version;
    priv->get_serial_number = xl_get_serial_number;
    priv->write_image_header = xl_write_image_header;

    g_autofree char *payload = g_malloc0(priv->max_packet_size);
    payload[0] = 0x02;
    hid_write(priv->handle, (const unsigned char *)payload, priv->max_packet_size);
}