#!/bin/sh

gcc `pkg-config --cflags gtk+-3.0 libusb-1.0` -o gtkdeck *.c `pkg-config --libs gtk+-3.0 libusb-1.0`