# OpenCPN Remote Plugin

This plugin hosts a browser-based remote view of OpenCPN without sharing the whole desktop. It captures only the OpenCPN application window and serves it to a responsive web page with pointer and keyboard input forwarded back to OpenCPN.

## What it does

- Runs as an OpenCPN plugin.
- Streams OpenCPN's own UI pixels to a web browser.
- Scales to phones, tablets, and desktops.
- Does not use VNC, RDP, or whole-screen capture.
- Protects the remote endpoint with a generated token.

## Build

Install OpenCPN and wxWidgets development packages first. On Debian/Ubuntu-style systems this is typically similar to:

```sh
sudo apt install build-essential cmake libwxgtk3.2-dev
```

Then build:

```sh
cmake -S . -B build
cmake --build build -j
```

For OpenCPN's **Install from file** / **Import plugin tarball** button, build the tarball:

```sh
cmake --build build --target plugin_tarball
```

Select the generated `.tar.gz` file from the `build` directory. Do not select the `.so` file directly; OpenCPN expects an import tarball with a root-level `metadata.xml`.

## Use

When enabled, the plugin logs a URL like:

```text
OpenCPN Remote listening on http://0.0.0.0:8765/?token=...
```

Open that URL from another device on the same network, replacing `0.0.0.0` with the host machine's LAN IP address.

## Security notes

This is a remote-control plugin. Use it only on trusted networks unless you place it behind HTTPS/VPN access control. The plugin intentionally captures the OpenCPN window only, but any sensitive information visible inside OpenCPN will be visible to authenticated browser clients.

## Current limitations

Some platforms or OpenGL driver combinations do not allow reliable application-window pixel capture through wxWidgets. If frames appear blank, disable OpenGL in OpenCPN or use a platform-specific capture backend as the next step.
