# Security policy

## Supported versions

The latest release on the default branch is the supported version. Please
upgrade before reporting a vulnerability when possible.

## Reporting a vulnerability

Please do not open a public issue for security-sensitive reports. Contact the
maintainer privately through the contact method listed on the project hosting
page and include:

- the affected version or commit;
- a minimal reproduction or clear steps;
- the distribution and relevant GTK/libevdev versions;
- the impact you observed.

Do not include private controller identifiers or unrelated personal data in a
report. Reports will be acknowledged as soon as practical and coordinated with
the reporter before public disclosure.

Miy Joy reads Linux input event devices read-only and does not need root for its
normal operation. The optional virtual-pad helper is separate and requires
access to `/dev/uinput`; it should only be used intentionally in a development
environment.
