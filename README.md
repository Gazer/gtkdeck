# GtkDeck

[Elgato Stream Deck](https://www.elgato.com/en/gaming/stream-deck) client for Linux

---
>Made with 💙 by Ricardo Markiewicz // [@gazeria](https://twitter.com/gazeria).

## Project Status:

🚨🚨 THIS IS A WORK IN PROGRESS 🚨🚨

Currently the following StreamDeck product variants are supported:
* StreamDeck Original V2
* StreamDeck Mini

Also support but not tested yet:
* StreamDeck Original
* StreamDeck XL

## Plugins

A plugin arquitecture is being created and is still in its early stages.

### Test Plugin

A dummy plugin to test the code. You can set a color and when you click the button it will be rendered with that color and back to the default image.

### System Plugin

* Open URL: Set an url and when the button is tap open the url in the default browser
* Write Text: Write a text in the current focused window.

### OBS Plugin

Work In Progress!!. You need to install and enable OBS Websocket plugin in OBS Studio.

* Chance Scene

## How to install

### udev setup

On linux, the udev subsystem blocks access to the StreamDeck without some special configuration.
Save the following to `/etc/udev/rules.d/50-elgato.rules` and reload the rules with `sudo udevadm control --reload-rules`

```
SUBSYSTEM=="input", GROUP="input", MODE="0666"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0063", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0090", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0060", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="006d", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="006c", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="008f", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0080", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0084", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="0086", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="009a", TAG+="uaccess"
SUBSYSTEM=="usb", ATTRS{idVendor}=="0fd9", ATTRS{idProduct}=="00a5", TAG+="uaccess"
```

Unplug and replug the device and it should be usable

### Install dependencies

#### Base requirements (Ubuntu/Debian)

```
$> apt install xvkbd meson libusb-1.0-0-dev libhidapi-dev libhidapi-libusb0 libgtk-3-dev libev-dev libjson-glib-dev
```

#### Media Control (Recommended)

For better media control support (Universal for Wayland/X11), install `playerctl`. If not found, it will fallback to `xvkbd`.

* **Ubuntu/Debian:** `sudo apt install playerctl`
* **Fedora:** `sudo dnf install playerctl`
* **Arch Linux:** `sudo pacman -S playerctl`
* **OpenSUSE:** `sudo zypper install playerctl`

### Compile

```
$> meson setup _build
$> ninja -C _build
```

### Run

```
$> ./_build/gtkdeck
```


<a target="_blank" href="https://icons8.com/icons/set/domain--v1">Website icon</a> icon by <a target="_blank" href="https://icons8.com">Icons8</a>
