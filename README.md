# GtkDeck

[Elgato Stream Deck](https://www.elgato.com/en/gaming/stream-deck) client for Linux

---
>Made with 💙 by Ricardo Markiewicz // [@gazeria](https://twitter.com/gazeria).

## Project Status:

🚨🚨 THIS IS A WORK IN PROGRESS 🚨🚨

Currently the following StreamDeck product variants are supported:
* StreamDeck Original V2
* StreamDeck Mini

## Plugins

A plugin arquitecture is being created and is still in its early stages.

### Test Plugin

A dummy plugin to test the code. You can set a color and when you click the button it will be rendered with that color and back to the default image.

### System Plugin

* Open URL: Set an url and when the button is tap open the url in the default browser
* Write Text: Write a text in the current focused window.


## How to install

### USB Description

### Linux

On linux, the udev subsystem blocks access to the StreamDeck without some special configuration.
Save the following to `/etc/udev/rules.d/50-elgato.rules` and reload the rules with `sudo udevadm control --reload-rules`

```
SUBSYSTEM=="input", GROUP="input", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0060", MODE:="666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0063", MODE:="666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="006c", MODE:="666", GROUP="plugdev"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="006d", MODE:="666", GROUP="plugdev"
```

Unplug and replug the device and it should be usable

### Install dependencies

$> apt install xvkbd meson libhidapi-dev libhidapi-libusb0 libgtk-3-dev

### Compile

$> meson setup _build
$> ninja -C _build

### Run

$> ./_build/gtkdeck