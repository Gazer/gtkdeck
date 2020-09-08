#!/bin/sh

gcc `pkg-config --cflags gtk+-3.0 libusb-1.0 hidapi-libusb` -o gtkdeck *.c system_plugin/*.c `pkg-config --libs gtk+-3.0 libusb-1.0 hidapi-libusb`