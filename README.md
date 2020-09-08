## Install dependencies

$> apt install xvkbd meson libhidapi-dev libhidapi-libusb0 libgtk-3-dev

## Compile

$> meson setup _build
$> ninja -C _build

## Run

$> ./_build/gtkdeck