#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <math.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void on_signal(int signum) {
  (void)signum;
  keep_running = 0;
}

static int emit_event(int fd, uint16_t type, uint16_t code, int32_t value) {
  struct input_event event = {0};
  gettimeofday(&event.time, NULL);
  event.type = type;
  event.code = code;
  event.value = value;

  for (int attempt = 0; attempt < 100; attempt++) {
    ssize_t written = write(fd, &event, sizeof(event));
    if (written == (ssize_t)sizeof(event))
      return 0;
    if (written < 0 && errno == EINTR)
      continue;
    if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      usleep(1000);
      continue;
    }
    return -1;
  }

  errno = EAGAIN;
  return -1;
}

static int emit_syn(int fd) { return emit_event(fd, EV_SYN, SYN_REPORT, 0); }

static int setup_abs(int fd, uint16_t code, int minimum, int maximum,
                     int flat) {
  struct uinput_abs_setup abs = {0};
  abs.code = code;
  abs.absinfo.minimum = minimum;
  abs.absinfo.maximum = maximum;
  abs.absinfo.flat = flat;
  return ioctl(fd, UI_ABS_SETUP, &abs);
}

static int setup_virtual_gamepad(int fd) {
  if (ioctl(fd, UI_SET_EVBIT, EV_KEY) < 0)
    return -1;
  if (ioctl(fd, UI_SET_EVBIT, EV_ABS) < 0)
    return -1;

  int buttons[] = {
      BTN_SOUTH, BTN_EAST,   BTN_NORTH,  BTN_WEST,   BTN_TL,
      BTN_TR,    BTN_TL2,    BTN_TR2,    BTN_SELECT, BTN_START,
      BTN_MODE,  BTN_THUMBL, BTN_THUMBR,
  };
  for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); i++) {
    if (ioctl(fd, UI_SET_KEYBIT, buttons[i]) < 0)
      return -1;
  }

  int axes[] = {ABS_X, ABS_Y,  ABS_RX,    ABS_RY,
                ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
  for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); i++) {
    if (ioctl(fd, UI_SET_ABSBIT, axes[i]) < 0)
      return -1;
  }

  if (setup_abs(fd, ABS_X, -32768, 32767, 1024) < 0)
    return -1;
  if (setup_abs(fd, ABS_Y, -32768, 32767, 1024) < 0)
    return -1;
  if (setup_abs(fd, ABS_RX, -32768, 32767, 1024) < 0)
    return -1;
  if (setup_abs(fd, ABS_RY, -32768, 32767, 1024) < 0)
    return -1;
  if (setup_abs(fd, ABS_Z, 0, 255, 0) < 0)
    return -1;
  if (setup_abs(fd, ABS_RZ, 0, 255, 0) < 0)
    return -1;
  if (setup_abs(fd, ABS_HAT0X, -1, 1, 0) < 0)
    return -1;
  if (setup_abs(fd, ABS_HAT0Y, -1, 1, 0) < 0)
    return -1;

  struct uinput_setup setup = {0};
  setup.id.bustype = BUS_VIRTUAL;
  setup.id.vendor = 0x1209;
  setup.id.product = 0x4d19;
  setup.id.version = 1;
  snprintf(setup.name, UINPUT_MAX_NAME_SIZE, "Miy Joy Virtual Pad");

  if (ioctl(fd, UI_DEV_SETUP, &setup) < 0)
    return -1;
  if (ioctl(fd, UI_DEV_CREATE) < 0)
    return -1;

  return 0;
}

int main(void) {
  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0) {
    fprintf(stderr, "Could not open /dev/uinput: %s\n", strerror(errno));
    return 1;
  }

  if (setup_virtual_gamepad(fd) < 0) {
    fprintf(stderr, "Could not create virtual gamepad: %s\n", strerror(errno));
    close(fd);
    return 1;
  }

  fprintf(stderr, "Virtual gamepad active: Miy Joy Virtual Pad\n");
  usleep(150000);

  int tick = 0;
  while (keep_running) {
    double phase = tick / 24.0;
    int x = (int)(sin(phase) * 32767.0);
    int y = (int)(cos(phase * 0.85) * 32767.0);
    int rx = (int)(sin(phase * 0.5) * 32767.0);
    int ry = (int)(cos(phase * 0.4) * 32767.0);
    int z = (tick * 3) % 256;
    int rz = 255 - z;
    int hatx = ((tick / 30) % 3) - 1;
    int haty = (((tick / 45) + 1) % 3) - 1;
    int south = (tick / 20) % 2;
    int east = (tick / 35) % 2;
    int tl = (tick / 50) % 2;
    int start = (tick / 80) % 2;

    if (emit_event(fd, EV_ABS, ABS_X, x) < 0 ||
        emit_event(fd, EV_ABS, ABS_Y, y) < 0 ||
        emit_event(fd, EV_ABS, ABS_RX, rx) < 0 ||
        emit_event(fd, EV_ABS, ABS_RY, ry) < 0 ||
        emit_event(fd, EV_ABS, ABS_Z, z) < 0 ||
        emit_event(fd, EV_ABS, ABS_RZ, rz) < 0 ||
        emit_event(fd, EV_ABS, ABS_HAT0X, hatx) < 0 ||
        emit_event(fd, EV_ABS, ABS_HAT0Y, haty) < 0 ||
        emit_event(fd, EV_KEY, BTN_SOUTH, south) < 0 ||
        emit_event(fd, EV_KEY, BTN_EAST, east) < 0 ||
        emit_event(fd, EV_KEY, BTN_TL, tl) < 0 ||
        emit_event(fd, EV_KEY, BTN_START, start) < 0 || emit_syn(fd) < 0) {
      fprintf(stderr, "Could not emit events: %s\n", strerror(errno));
      break;
    }

    tick++;
    usleep(16000);
  }

  ioctl(fd, UI_DEV_DESTROY);
  close(fd);
  return 0;
}
