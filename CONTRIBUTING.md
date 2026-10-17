# Contributing to Miy Joy

Thanks for helping improve Miy Joy. Small, focused pull requests are easiest to
review.

## Development setup

Install GTK4, libevdev, libudev, Meson, Ninja, and a C11 compiler. Then run:

```sh
meson setup build --buildtype=debugoptimized
meson compile -C build
meson test -C build --print-errorlogs
```

The normal test suite does not require a physical controller. The optional
`miy-joy-virtual-pad` helper creates a real `/dev/uinput` device and should only
be run on a development machine where synthetic input is expected.

## Before opening a pull request

- keep changes focused and explain user-visible behavior;
- run the build, Meson tests, and `./build/miy-joy --list`;
- validate desktop metadata when changing files under `data/`;
- do not add broad `0666` udev rules or require root for normal operation;
- document hardware-specific behavior and permission assumptions.

Please include the exact distribution and hardware details for input-device
bugs. Never include personal device paths or logs containing sensitive data.
