# Miy Joy

Miy Joy is a small GTK4 joystick/gamepad tester for Linux.

It aims to be a lightweight `joy.cpl`-style utility: pick a controller from a
compact list, open its details, then watch the X/Y quadrant, POV hat, axes, and
buttons in real time.

## Status: early and barely tested

> **Please read before trusting it.** Miy Joy is early software and has had very
> little testing.
>
> - **Tested:** one Microsoft Xbox Series S|X controller, over USB, on Arch Linux
>   with GNOME (Wayland). That is the whole list.
> - **Should work, never tried:** any other gamepad or joystick the kernel exposes
>   through `/dev/input/event*` (it reads evdev, which covers most HID game
>   devices), Bluetooth controllers, DualShock/DualSense touchpads, flight sticks
>   and wheels.
> - **Missing:** calibration and dead-zone settings, button remapping, showing how
>   SDL/Steam see a controller, force-feedback beyond a single rumble test,
>   several controllers side by side, and packages for any distribution
>   (Flatpak, AUR, deb...). It is Linux only.
> - The Spanish and Portuguese texts have not been reviewed by anyone else.
>
> Bug reports with the output of `miy-joy --list` are very welcome.

## Features

- GTK4 UI that follows the desktop light/dark theme.
- Compact controller list with a separate details window.
- Real input reading through `/dev/input/event*` with libevdev.
- Touchpad position and contact visualisation for controllers that export a
  separate touch interface (including the usual Linux DualShock 4 layout).
- udev refresh on controller add/remove/change events.
- Bus labeling in the device list, such as USB or Bluetooth.
- Tiny built-in locale switch for English, Spanish, and Portuguese.
- Last-pressed button readout and per-button tooltips with the evdev name
  (`BTN_SOUTH`, …) and its Xbox-style name, handy for mapping in other programs.
- Vibration test button for controllers that support rumble (needs write
  access to the device node, which the desktop seat ACL normally grants;
  without it the button is hidden and everything else still works).
- `--list` command for quick terminal diagnostics.
- `miy-joy-virtual-pad` helper to create a virtual Linux gamepad for testing.

## Build

```sh
meson setup build --buildtype=debugoptimized
meson compile -C build
meson test -C build --print-errorlogs
./build/miy-joy
```

For a release build, use `--buildtype=release`. To install into a staging
prefix instead of the system, pass for example
`--prefix="$HOME/.local"` during setup and run `meson install -C build`.

To spawn a virtual controller for testing:

```sh
./build/miy-joy-virtual-pad
```

The helper creates a synthetic `/dev/uinput` device, so it needs the same
session/device permissions as other uinput tools. It is optional and should
only be run intentionally; the main application only reads evdev devices.

## Install

From source, for your user only (no root needed):

```sh
meson setup build --buildtype=release --prefix="$HOME/.local"
meson compile -C build
meson install -C build
```

The installed launcher points at the absolute path of the binary, so it works
from the desktop menu even when `~/.local/bin` is not in the session's `PATH`.

## Releases

Tagged versions are built and published by CI as **pre-releases marked as
untested**, with a source archive, an x86_64 binary archive built on Ubuntu
24.04, and checksums. Prefer the source archive. The binary archive has only been
checked to start on the machine described in [Status](#status-early-and-barely-tested)
and will not necessarily run on other distributions. See
[Releases](https://github.com/Javiju555/miy-joy/releases).

## Dependencies

- GTK 4 (>= 4.10)
- libevdev
- libudev
- Meson + Ninja

On many distributions the user must be allowed to read `/dev/input/event*`.
Prefer the desktop session's seat/ACL permissions. If a controller is missing,
inspect its node and ACL with commands such as:

```sh
ls -l /dev/input/event*
getfacl /dev/input/eventN
udevadm info --query=property --name=/dev/input/eventN
```

Miy Joy never needs root and does not grab input devices. Avoid broad `0666`
udev rules; use the distribution's seat policy or a narrowly scoped rule when
necessary.

## Development and contributions

The CI runs GCC and Clang builds, Meson smoke tests, desktop/AppStream metadata
validation, staged installation, and an AddressSanitizer/UndefinedBehaviorSanitizer
build. See [CONTRIBUTING.md](CONTRIBUTING.md) for the local workflow and
[SECURITY.md](SECURITY.md) for private vulnerability reports.

## Touchpads on controllers

The panel on a DualShock 4/DualSense is a genuine capacitive touch surface, not
the same thing as the laptop's built-in touchpad.  Linux commonly exposes it as
an additional `/dev/input/event*` node alongside the gamepad node. Miy Joy
matches the interfaces through their shared HID parent, so it only attaches a
touchpad that belongs to the selected controller; an unrelated laptop touchpad
is never listed as a controller touchpad.

The detail view shows up to two contacts, their position, and whether the pad
is being touched when the driver exports absolute touch coordinates. Some
third-party controllers expose the panel only as mouse motion, or do not expose
it at all; in that case the main controller remains fully testable and the
touch panel reports that it is not available.

## License

MIT
