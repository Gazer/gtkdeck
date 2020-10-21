
// MINI SPECIFIC FUNCTIONS

unsigned char mini_reset_bytes[] = {0x0B, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

GBytes *mini_reset_command(StreamDeck *deck) { return g_bytes_new_static(mini_reset_bytes, 17); }

GBytes *mini_brightness_command(StreamDeck *self, int percentage) {
    unsigned char *bytes = g_malloc0(sizeof(unsigned char) * 17);

    bytes[0] = 0x05;
    bytes[1] = 0x55;
    bytes[2] = 0xaa;
    bytes[3] = 0xd1;
    bytes[4] = 0x01;
    bytes[5] = percentage;

    return g_bytes_new(bytes, 17);
}

GString *mini_get_firmware_version(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    return read_feature_string(priv->handle, 0x04, 17, 5);
}

GString *mini_get_serial_number(StreamDeck *self) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(self);
    return read_feature_string(priv->handle, 0x03, 17, 5);
}

void mini_write_image_header(unsigned char *packet, int key, int this_length, int bytes_remaining,
                             int page_number) {
    packet[0] = 0x02;
    packet[1] = 0x01;
    packet[2] = page_number;
    packet[3] = 0;
    packet[4] = this_length == bytes_remaining ? 1 : 0;
    packet[5] = key + 1;
    packet[6] = 0;
    packet[7] = 0;
    packet[8] = 0;
    packet[9] = 0;
    packet[10] = 0;
    packet[11] = 0;
    packet[12] = 0;
    packet[13] = 0;
    packet[14] = 0;
    packet[15] = 0;
}

void stream_deck_init_mini(StreamDeck *deck) {
    StreamDeckPrivate *priv = stream_deck_get_instance_private(deck);

    priv->rows = 2;
    priv->columns = 3;
    priv->icon_size = 80;
    priv->key_direction = LTR;
    priv->key_data_offset = 4;
    priv->key_flip_h = 0;
    priv->key_flip_v = 1;
    priv->key_rotation = GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE;
    priv->key_image_format = "bmp";
    priv->key_read_header = 1;
    priv->max_packet_size = 1024;
    priv->packet_header_size = 16;
    priv->reset_command = mini_reset_command;
    priv->brightness_command = mini_brightness_command;
    priv->get_firmware_version = mini_get_firmware_version;
    priv->get_serial_number = mini_get_serial_number;
    priv->write_image_header = mini_write_image_header;
}