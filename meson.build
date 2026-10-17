project(
  'miy-joy',
  'c',
  version: '0.3.0',
  license: 'MIT',
  default_options: [
    'c_std=c11',
    'warning_level=2',
  ],
)

cc = meson.get_compiler('c')

gtk4_dep = dependency('gtk4', version: '>=4.10')
glib_dep = dependency('glib-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
libevdev_dep = dependency('libevdev')
libudev_dep = dependency('libudev')
math_dep = cc.find_library('m', required: true)

miy_joy_exe = executable(
  'miy-joy',
  'src/main.c',
  dependencies: [
    gtk4_dep,
    glib_dep,
    gio_unix_dep,
    libevdev_dep,
    libudev_dep,
  ],
  install: true,
)

test('miy-joy --version', miy_joy_exe, args: ['--version'])
test('miy-joy --help', miy_joy_exe, args: ['--help'])

logic_tests = executable(
  'miy-joy-test-logic',
  'tests/test_logic.c',
  dependencies: [glib_dep],
)
test('logic helpers', logic_tests)

miy_virtual_pad_exe = executable(
  'miy-joy-virtual-pad',
  'src/virtual_pad.c',
  dependencies: [
    math_dep,
  ],
  install: true,
)

# The desktop session's PATH often lacks ~/.local/bin, so a bare "Exec=miy-joy"
# cannot be launched from the menu. Install it with the absolute binary path.
desktop_file = configure_file(
  input: 'data/io.github.javiju.MiyJoy.desktop',
  output: 'io.github.javiju.MiyJoy.desktop',
  command: [
    'sed',
    's|^Exec=miy-joy|Exec=' + (get_option('prefix') / get_option('bindir') / 'miy-joy') + '|',
    '@INPUT@',
  ],
  capture: true,
  install: true,
  install_dir: get_option('datadir') / 'applications',
  install_tag: 'runtime',
)
install_data(
  'data/io.github.javiju.MiyJoy.metainfo.xml',
  install_dir: get_option('datadir') / 'metainfo',
  install_tag: 'runtime',
)
install_data(
  'data/icons/hicolor/scalable/apps/io.github.javiju.MiyJoy.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
  install_tag: 'runtime',
)
