#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <linux/input.h>

#include <glib-unix.h>
#include <gtk/gtk.h>
#include <libevdev/libevdev.h>
#include <libudev.h>

#include "logic.h"

#define APP_ID "io.github.javiju.MiyJoy"
#define APP_VERSION "0.3.0"
#define MAX_AXES ABS_CNT
#define MAX_KEYS KEY_CNT

typedef enum {
  UI_EN,
  UI_ES,
  UI_PT,
} UiLanguage;

typedef struct {
  char *name;
  char *path;
  char *bus;
  char *group;
  char *touchpad_path;
  unsigned int vendor;
  unsigned int product;
  int axis_count;
  int button_count;
} DeviceInfo;

typedef struct {
  char *path;
  char *group;
} TouchpadInfo;

typedef struct {
  int code;
  int minimum;
  int maximum;
  int flat;
  int value;
  GtkWidget *bar;
  GtkWidget *value_label;
} AxisRow;

typedef struct {
  int code;
  int number;
  int value;
  GtkWidget *cell;
} ButtonCell;

typedef struct {
  GtkApplication *application;
  GtkWidget *window;
  GtkWidget *details_window;
  GtkWidget *device_list;
  GtkWidget *details_button;
  GtkWidget *status_label;
  GtkWidget *device_info_label;
  GtkWidget *xy_area;
  GtkWidget *hat_area;
  GtkWidget *xy_label;
  GtkWidget *hat_label;
  GtkWidget *touch_area;
  GtkWidget *touch_label;
  GtkWidget *axes_box;
  GtkWidget *buttons_grid;
  GtkWidget *empty_label;
  GtkWidget *refresh_button;
  GtkWidget *last_button_label;
  GtkWidget *rumble_button;

  GPtrArray *devices;
  GPtrArray *axis_rows;
  GPtrArray *button_cells;

  struct udev *udev;
  struct udev_monitor *udev_monitor;
  struct libevdev *evdev;
  int fd;
  bool fd_writable;
  int ff_effect_id;
  guint fd_source;
  struct libevdev *touch_evdev;
  int touch_fd;
  guint touch_fd_source;
  guint udev_source;
  int selected_index;
  bool reloading_devices;

  int axis_values[MAX_AXES];
  int axis_minimum[MAX_AXES];
  int axis_maximum[MAX_AXES];
  bool axis_present[MAX_AXES];
  int key_values[MAX_KEYS];
  bool key_present[MAX_KEYS];

  int touch_x[2];
  int touch_y[2];
  int touch_x_code;
  int touch_y_code;
  int touch_x_minimum;
  int touch_x_maximum;
  int touch_y_minimum;
  int touch_y_maximum;
  bool touch_present;
  bool touch_pressed;
  bool touch_active[2];
  bool touch_multitouch;
  int touch_slot;

  UiLanguage lang;
} AppState;

static void reload_devices(AppState *app);
static void close_touch_device(AppState *app);
static void close_selected_device(AppState *app);

static const char *t(AppState *app, const char *key) {
  const UiLanguage lang = app ? app->lang : UI_EN;

  if (strcmp(key, "app_title") == 0)
    return "Miy Joy";
  if (strcmp(key, "subtitle") == 0) {
    if (lang == UI_ES)
      return "Probador ligero de mandos";
    if (lang == UI_PT)
      return "Testador leve de comandos";
    return "Lightweight controller tester";
  }
  if (strcmp(key, "refresh") == 0) {
    if (lang == UI_ES)
      return "Actualizar";
    if (lang == UI_PT)
      return "Atualizar";
    return "Refresh";
  }
  if (strcmp(key, "details") == 0) {
    if (lang == UI_ES)
      return "Detalles";
    if (lang == UI_PT)
      return "Detalhes";
    return "Details";
  }
  if (strcmp(key, "details_title") == 0) {
    if (lang == UI_ES)
      return "Miy Joy - Detalles";
    if (lang == UI_PT)
      return "Miy Joy - Detalhes";
    return "Miy Joy - Details";
  }
  if (strcmp(key, "close") == 0) {
    if (lang == UI_ES)
      return "Cerrar";
    if (lang == UI_PT)
      return "Fechar";
    return "Close";
  }
  if (strcmp(key, "controllers") == 0) {
    if (lang == UI_ES)
      return "Mandos instalados";
    if (lang == UI_PT)
      return "Comandos instalados";
    return "Installed controllers";
  }
  if (strcmp(key, "no_devices") == 0) {
    if (lang == UI_ES)
      return "No se han encontrado mandos";
    if (lang == UI_PT)
      return "Nenhum comando encontrado";
    return "No controllers found";
  }
  if (strcmp(key, "select_device") == 0) {
    if (lang == UI_ES)
      return "Selecciona un mando";
    if (lang == UI_PT)
      return "Selecione um comando";
    return "Select a controller";
  }
  if (strcmp(key, "permission_hint") == 0) {
    if (lang == UI_ES)
      return "Sin permiso para leer el dispositivo";
    if (lang == UI_PT)
      return "Sem permissao para ler o dispositivo";
    return "No permission to read device";
  }
  if (strcmp(key, "open_failed") == 0) {
    if (lang == UI_ES)
      return "No se pudo abrir el dispositivo";
    if (lang == UI_PT)
      return "Nao foi possivel abrir o dispositivo";
    return "Could not open device";
  }
  if (strcmp(key, "listening") == 0) {
    if (lang == UI_ES)
      return "Leyendo eventos";
    if (lang == UI_PT)
      return "Lendo eventos";
    return "Reading events";
  }
  if (strcmp(key, "disconnected") == 0) {
    if (lang == UI_ES)
      return "Dispositivo desconectado";
    if (lang == UI_PT)
      return "Dispositivo desconectado";
    return "Device disconnected";
  }
  if (strcmp(key, "hotplug_unavailable") == 0) {
    if (lang == UI_ES)
      return "Actualización automática no disponible; usa Actualizar";
    if (lang == UI_PT)
      return "Atualizacao automatica indisponivel; use Atualizar";
    return "Automatic refresh unavailable; use Refresh";
  }
  if (strcmp(key, "axes") == 0) {
    if (lang == UI_ES)
      return "Ejes";
    if (lang == UI_PT)
      return "Eixos";
    return "Axes";
  }
  if (strcmp(key, "buttons") == 0) {
    if (lang == UI_ES)
      return "Botones";
    if (lang == UI_PT)
      return "Botoes";
    return "Buttons";
  }
  if (strcmp(key, "last_button") == 0) {
    if (lang == UI_ES)
      return "Último botón";
    if (lang == UI_PT)
      return "Último botão";
    return "Last button";
  }
  if (strcmp(key, "last_button_none") == 0) {
    if (lang == UI_ES)
      return "Pulsa un botón del mando";
    if (lang == UI_PT)
      return "Prima um botão do comando";
    return "Press a controller button";
  }
  if (strcmp(key, "button_word") == 0) {
    if (lang == UI_ES)
      return "Botón";
    if (lang == UI_PT)
      return "Botão";
    return "Button";
  }
  if (strcmp(key, "rumble") == 0) {
    if (lang == UI_ES)
      return "Probar vibración";
    if (lang == UI_PT)
      return "Testar vibração";
    return "Test vibration";
  }
  if (strcmp(key, "rumble_failed") == 0) {
    if (lang == UI_ES)
      return "No se pudo hacer vibrar el mando";
    if (lang == UI_PT)
      return "Não foi possível fazer vibrar o comando";
    return "Could not rumble the controller";
  }
  if (strcmp(key, "xy") == 0)
    return "X / Y";
  if (strcmp(key, "pov") == 0)
    return "POV";
  if (strcmp(key, "touchpad") == 0) {
    if (lang == UI_ES)
      return "Panel táctil";
    if (lang == UI_PT)
      return "Painel tátil";
    return "Touchpad";
  }
  if (strcmp(key, "no_touchpad") == 0) {
    if (lang == UI_ES)
      return "Este mando no expone un panel táctil";
    if (lang == UI_PT)
      return "Este comando não expõe um painel tátil";
    return "This controller does not expose a touchpad";
  }
  if (strcmp(key, "touch_idle") == 0) {
    if (lang == UI_ES)
      return "Sin contacto";
    if (lang == UI_PT)
      return "Sem contacto";
    return "No contact";
  }
  if (strcmp(key, "idle_xy") == 0)
    return "X:      0 (+0.000)\nY:      0 (+0.000)";
  if (strcmp(key, "idle_hat") == 0)
    return "HAT  0 /  0";

  return key;
}

static UiLanguage detect_language(void) {
  const char *locale = setlocale(LC_MESSAGES, NULL);
  if (!locale)
    locale = g_getenv("LANG");
  if (locale && g_str_has_prefix(locale, "es"))
    return UI_ES;
  if (locale && g_str_has_prefix(locale, "pt"))
    return UI_PT;
  return UI_EN;
}

static void device_info_free(gpointer data) {
  DeviceInfo *info = data;
  if (!info)
    return;
  g_free(info->name);
  g_free(info->path);
  g_free(info->bus);
  g_free(info->group);
  g_free(info->touchpad_path);
  g_free(info);
}

static void touchpad_info_free(gpointer data) {
  TouchpadInfo *info = data;
  if (!info)
    return;
  g_free(info->path);
  g_free(info->group);
  g_free(info);
}

static const char *describe_bus(const char *bus) {
  if (!bus || !*bus)
    return "input";
  if (strcmp(bus, "usb") == 0)
    return "USB";
  if (strcmp(bus, "bluetooth") == 0)
    return "Bluetooth";
  if (strcmp(bus, "i2c") == 0)
    return "I2C";
  if (strcmp(bus, "platform") == 0)
    return "Platform";
  return bus;
}

static void clear_box(GtkWidget *box) {
  GtkWidget *child = gtk_widget_get_first_child(box);
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_box_remove(GTK_BOX(box), child);
    child = next;
  }
}

static void clear_grid(GtkWidget *grid) {
  GtkWidget *child = gtk_widget_get_first_child(grid);
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_grid_remove(GTK_GRID(grid), child);
    child = next;
  }
}

static void clear_list_box(GtkWidget *list_box) {
  GtkWidget *child = gtk_widget_get_first_child(list_box);
  while (child) {
    GtkWidget *next = gtk_widget_get_next_sibling(child);
    gtk_list_box_remove(GTK_LIST_BOX(list_box), child);
    child = next;
  }
}

static bool has_abs(struct libevdev *dev, int code) {
  return libevdev_has_event_code(dev, EV_ABS, code) != 0;
}

static bool has_key(struct libevdev *dev, int code) {
  return libevdev_has_event_code(dev, EV_KEY, code) != 0;
}

static bool udev_property_is_true(struct udev_device *device,
                                  const char *property) {
  return g_strcmp0(udev_device_get_property_value(device, property), "1") == 0;
}

/* Event interfaces belonging to the same physical HID device share this
 * parent.  A DualShock 4 normally exposes its gamepad and touchpad through
 * separate event nodes, so the HID parent is the useful link between them. */
static char *device_group_key(struct udev_device *device) {
  struct udev_device *hid =
      udev_device_get_parent_with_subsystem_devtype(device, "hid", NULL);
  if (hid)
    return g_strdup(udev_device_get_syspath(hid));

  const char *path = udev_device_get_property_value(device, "ID_PATH");
  if (path && *path)
    return g_strdup(path);
  return NULL;
}

static bool is_touchpad_like(struct libevdev *dev, bool marked_touchpad) {
  bool has_multitouch_coordinates = has_abs(dev, ABS_MT_POSITION_X) &&
                                     has_abs(dev, ABS_MT_POSITION_Y);
  bool has_single_touch_coordinates = has_abs(dev, ABS_X) && has_abs(dev, ABS_Y);
  if (marked_touchpad)
    return has_multitouch_coordinates || has_single_touch_coordinates;
  return has_multitouch_coordinates &&
         (has_key(dev, BTN_TOUCH) || has_key(dev, BTN_TOOL_FINGER));
}

static bool is_controller_like(struct libevdev *dev, int *axis_count_out,
                               int *button_count_out) {
  int axes = 0;
  int buttons = 0;
  bool has_hat = false;

  for (int code = 0; code < ABS_CNT; code++) {
    if (has_abs(dev, code)) {
      axes++;
      has_hat = has_hat || is_hat_axis(code);
    }
  }

  for (int code = 0; code < KEY_CNT; code++) {
    if (is_game_button(code) && has_key(dev, code))
      buttons++;
  }

  if (axis_count_out)
    *axis_count_out = axes;
  if (button_count_out)
    *button_count_out = buttons;

  return axes > 0 || buttons > 0 || has_hat;
}

static GPtrArray *scan_devices(void) {
  GPtrArray *devices = g_ptr_array_new_with_free_func(device_info_free);
  GPtrArray *touchpads = g_ptr_array_new_with_free_func(touchpad_info_free);
  struct udev *udev = udev_new();
  if (!udev)
    goto done;

  struct udev_enumerate *enumerate = udev_enumerate_new(udev);
  if (!enumerate) {
    udev_unref(udev);
    goto done;
  }

  udev_enumerate_add_match_subsystem(enumerate, "input");
  if (udev_enumerate_scan_devices(enumerate) < 0) {
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    goto done;
  }

  struct udev_list_entry *entries = udev_enumerate_get_list_entry(enumerate);
  struct udev_list_entry *entry = NULL;

  udev_list_entry_foreach(entry, entries) {
    const char *syspath = udev_list_entry_get_name(entry);
    struct udev_device *udev_device =
        udev_device_new_from_syspath(udev, syspath);
    if (!udev_device)
      continue;

    const char *devnode = udev_device_get_devnode(udev_device);
    if (!devnode || !g_str_has_prefix(devnode, "/dev/input/event")) {
      udev_device_unref(udev_device);
      continue;
    }

    int fd = open(devnode, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
      udev_device_unref(udev_device);
      continue;
    }

    struct libevdev *evdev = NULL;
    if (libevdev_new_from_fd(fd, &evdev) == 0) {
      int axes = 0;
      int buttons = 0;
      const char *is_joystick =
          udev_device_get_property_value(udev_device, "ID_INPUT_JOYSTICK");
      const char *is_gamepad =
          udev_device_get_property_value(udev_device, "ID_INPUT_GAMEPAD");
      bool marked_controller =
          g_strcmp0(is_joystick, "1") == 0 || g_strcmp0(is_gamepad, "1") == 0;
      bool marked_pointer =
          udev_property_is_true(udev_device, "ID_INPUT_MOUSE") ||
          udev_property_is_true(udev_device, "ID_INPUT_TOUCHPAD") ||
          udev_property_is_true(udev_device, "ID_INPUT_TOUCHSCREEN") ||
          udev_property_is_true(udev_device, "ID_INPUT_TABLET") ||
          udev_property_is_true(udev_device, "ID_INPUT_KEYBOARD");

      bool marked_touchpad =
          g_strcmp0(udev_device_get_property_value(udev_device,
                                                    "ID_INPUT_TOUCHPAD"),
                     "1") == 0;

      if ((marked_controller || !marked_pointer) &&
          is_controller_like(evdev, &axes, &buttons)) {
        DeviceInfo *info = g_new0(DeviceInfo, 1);
        const char *name = libevdev_get_name(evdev);
        const char *bus = udev_device_get_property_value(udev_device, "ID_BUS");
        info->name = g_strdup(name && *name ? name : devnode);
        info->path = g_strdup(devnode);
        info->bus = g_strdup(describe_bus(bus));
        info->group = device_group_key(udev_device);
        info->vendor = libevdev_get_id_vendor(evdev);
        info->product = libevdev_get_id_product(evdev);
        info->axis_count = axes;
        info->button_count = buttons;
        g_ptr_array_add(devices, info);
      } else if (is_touchpad_like(evdev, marked_touchpad)) {
        TouchpadInfo *touchpad = g_new0(TouchpadInfo, 1);
        touchpad->path = g_strdup(devnode);
        touchpad->group = device_group_key(udev_device);
        g_ptr_array_add(touchpads, touchpad);
      }
      libevdev_free(evdev);
    }

    close(fd);
    udev_device_unref(udev_device);
  }

  udev_enumerate_unref(enumerate);
  udev_unref(udev);

  for (guint i = 0; i < devices->len; i++) {
    DeviceInfo *controller = g_ptr_array_index(devices, i);
    for (guint j = 0; j < touchpads->len; j++) {
      TouchpadInfo *touchpad = g_ptr_array_index(touchpads, j);
      if (controller->group && touchpad->group &&
          g_strcmp0(controller->group, touchpad->group) == 0) {
        controller->touchpad_path = g_strdup(touchpad->path);
        break;
      }
    }
  }

done:
  g_ptr_array_unref(touchpads);
  return devices;
}

static void set_status(AppState *app, const char *message) {
  gtk_label_set_text(GTK_LABEL(app->status_label), message);
}

static AxisRow *find_axis_row(AppState *app, int code) {
  for (guint i = 0; i < app->axis_rows->len; i++) {
    AxisRow *row = g_ptr_array_index(app->axis_rows, i);
    if (row->code == code)
      return row;
  }
  return NULL;
}

static ButtonCell *find_button_cell(AppState *app, int code) {
  for (guint i = 0; i < app->button_cells->len; i++) {
    ButtonCell *cell = g_ptr_array_index(app->button_cells, i);
    if (cell->code == code)
      return cell;
  }
  return NULL;
}

static int axis_value(AppState *app, int code) {
  if (code < 0 || code >= MAX_AXES || !app->axis_present[code])
    return 0;
  return app->axis_values[code];
}

static double axis_norm(AppState *app, int code) {
  if (code < 0 || code >= MAX_AXES || !app->axis_present[code])
    return 0.0;
  return normalize_axis(app->axis_values[code], app->axis_minimum[code], app->axis_maximum[code]);
}

static void queue_visuals(AppState *app) {
  if (app->xy_area)
    gtk_widget_queue_draw(app->xy_area);
  if (app->hat_area)
    gtk_widget_queue_draw(app->hat_area);
  if (app->touch_area)
    gtk_widget_queue_draw(app->touch_area);
}

static void update_touch_label(AppState *app) {
  if (!app->touch_label)
    return;
  if (!app->touch_evdev) {
    gtk_label_set_text(GTK_LABEL(app->touch_label), t(app, "no_touchpad"));
    return;
  }

  int active_count = (app->touch_active[0] ? 1 : 0) +
                     (app->touch_active[1] ? 1 : 0);
  if (!app->touch_present || active_count == 0) {
    gtk_label_set_text(GTK_LABEL(app->touch_label), t(app, "touch_idle"));
    return;
  }

  int slot = app->touch_active[0] ? 0 : 1;
  char text[96];
  snprintf(text, sizeof(text), "%s %d/2  •  X: %d  Y: %d",
           app->touch_pressed ? "●" : "○", active_count, app->touch_x[slot],
           app->touch_y[slot]);
  gtk_label_set_text(GTK_LABEL(app->touch_label), text);
}

static void setup_touchpad_state(AppState *app) {
  app->touch_x_code = has_abs(app->touch_evdev, ABS_MT_POSITION_X)
                          ? ABS_MT_POSITION_X
                          : ABS_X;
  app->touch_y_code = has_abs(app->touch_evdev, ABS_MT_POSITION_Y)
                          ? ABS_MT_POSITION_Y
                          : ABS_Y;
  const struct input_absinfo *x =
      libevdev_get_abs_info(app->touch_evdev, app->touch_x_code);
  const struct input_absinfo *y =
      libevdev_get_abs_info(app->touch_evdev, app->touch_y_code);
  if (!x || !y)
    return;

  app->touch_x[0] = x->value;
  app->touch_y[0] = y->value;
  app->touch_x_minimum = x->minimum;
  app->touch_x_maximum = x->maximum;
  app->touch_y_minimum = y->minimum;
  app->touch_y_maximum = y->maximum;
  app->touch_present = true;
  app->touch_multitouch = has_abs(app->touch_evdev, ABS_MT_SLOT);
  app->touch_slot = 0;
  app->touch_pressed = libevdev_get_event_value(app->touch_evdev, EV_KEY, BTN_TOUCH) ||
                       libevdev_get_event_value(app->touch_evdev, EV_KEY,
                                                 BTN_TOOL_FINGER);
  app->touch_active[0] = app->touch_pressed;
}

static void update_axis_row(AppState *app, AxisRow *row) {
  (void)app;

  double norm = normalize_axis(row->value, row->minimum, row->maximum);
  double fraction = (norm + 1.0) / 2.0;
  char text[96];

  snprintf(text, sizeof(text), "%d  (%+.3f)", row->value, norm);
  gtk_label_set_text(GTK_LABEL(row->value_label), text);
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(row->bar), fraction);
}

static void update_button_cell(ButtonCell *cell) {
  if (cell->value) {
    gtk_widget_add_css_class(cell->cell, "pressed");
  } else {
    gtk_widget_remove_css_class(cell->cell, "pressed");
  }
}

static void update_summary_labels(AppState *app) {
  char xy[128];
  char hat[64];
  /* Same two-line shape as the idle text so the panel never changes size. */
  snprintf(xy, sizeof(xy), "X: %6d (%+.3f)\nY: %6d (%+.3f)",
           axis_value(app, ABS_X), axis_norm(app, ABS_X),
           axis_value(app, ABS_Y), axis_norm(app, ABS_Y));
  snprintf(hat, sizeof(hat), "HAT %2d / %2d", axis_value(app, ABS_HAT0X),
           axis_value(app, ABS_HAT0Y));
  gtk_label_set_text(GTK_LABEL(app->xy_label), xy);
  gtk_label_set_text(GTK_LABEL(app->hat_label), hat);
}

/* "Button 1 - BTN_SOUTH (Xbox: A)": the evdev name is what mapping tools and
 * other programs report, so it is the useful thing to show next to the number. */
static char *describe_button(AppState *app, int code, int number) {
  const char *evdev_name = libevdev_event_code_get_name(EV_KEY, code);
  const char *xbox_name = xbox_button_name(code);
  char fallback[16];

  if (!evdev_name) {
    snprintf(fallback, sizeof(fallback), "0x%x", (unsigned int)code);
    evdev_name = fallback;
  }
  if (xbox_name)
    return g_strdup_printf("%s %d - %s (Xbox: %s)", t(app, "button_word"),
                           number, evdev_name, xbox_name);
  return g_strdup_printf("%s %d - %s", t(app, "button_word"), number,
                         evdev_name);
}

static void stop_rumble_effect(AppState *app) {
  if (app->fd >= 0 && app->ff_effect_id >= 0)
    (void)ioctl(app->fd, EVIOCRMFF, app->ff_effect_id);
  app->ff_effect_id = -1;
}

static void on_rumble_clicked(GtkButton *button, gpointer user_data) {
  AppState *app = user_data;
  (void)button;

  if (app->fd < 0 || !app->fd_writable)
    return;

  struct ff_effect effect;
  memset(&effect, 0, sizeof(effect));
  effect.type = FF_RUMBLE;
  effect.id = (short)app->ff_effect_id;
  effect.u.rumble.strong_magnitude = 0xc000;
  effect.u.rumble.weak_magnitude = 0x8000;
  effect.replay.length = 700;

  struct input_event play;
  memset(&play, 0, sizeof(play));
  play.type = EV_FF;

  if (ioctl(app->fd, EVIOCSFF, &effect) < 0) {
    set_status(app, t(app, "rumble_failed"));
    return;
  }
  app->ff_effect_id = effect.id;
  play.code = (uint16_t)effect.id;
  play.value = 1;
  if (write(app->fd, &play, sizeof(play)) != (ssize_t)sizeof(play))
    set_status(app, t(app, "rumble_failed"));
}

static void apply_event(AppState *app, const struct input_event *event) {
  if (event->type == EV_ABS && event->code < MAX_AXES) {
    app->axis_values[event->code] = event->value;
    app->axis_present[event->code] = true;

    AxisRow *row = find_axis_row(app, event->code);
    if (row) {
      row->value = event->value;
      update_axis_row(app, row);
    }
    update_summary_labels(app);
    queue_visuals(app);
  } else if (event->type == EV_KEY && event->code < MAX_KEYS) {
    app->key_values[event->code] = event->value;
    app->key_present[event->code] = true;

    ButtonCell *cell = find_button_cell(app, event->code);
    if (cell) {
      cell->value = event->value;
      update_button_cell(cell);
      if (event->value == 1 && app->last_button_label) {
        char *text = describe_button(app, cell->code, cell->number);
        gtk_label_set_text(GTK_LABEL(app->last_button_label), text);
        g_free(text);
      }
    }
  }
}

static void apply_touch_event(AppState *app, const struct input_event *event) {
  if (event->type == EV_ABS && event->code == ABS_MT_SLOT) {
    app->touch_slot = CLAMP(event->value, 0, 1);
  } else if (event->type == EV_ABS && event->code == ABS_MT_TRACKING_ID) {
    app->touch_active[app->touch_slot] = event->value >= 0;
  } else if (event->type == EV_ABS && event->code == app->touch_x_code) {
    app->touch_x[app->touch_slot] = event->value;
    app->touch_present = true;
  } else if (event->type == EV_ABS && event->code == app->touch_y_code) {
    app->touch_y[app->touch_slot] = event->value;
    app->touch_present = true;
  } else if (event->type == EV_KEY &&
             (event->code == BTN_TOUCH || event->code == BTN_TOOL_FINGER)) {
    app->touch_pressed = event->value != 0;
    if (!app->touch_multitouch)
      app->touch_active[0] = app->touch_pressed;
  }
  if (app->touch_multitouch)
    app->touch_pressed = app->touch_active[0] || app->touch_active[1];
  update_touch_label(app);
  queue_visuals(app);
}

static gboolean on_fd_event(gint fd, GIOCondition condition,
                            gpointer user_data) {
  AppState *app = user_data;

  if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
    set_status(app, t(app, "disconnected"));
    app->fd_source = 0;
    close_selected_device(app);
    app->touch_present = false;
    app->touch_pressed = false;
    memset(app->touch_active, 0, sizeof(app->touch_active));
    update_touch_label(app);
    queue_visuals(app);
    return G_SOURCE_REMOVE;
  }

  unsigned int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
  while (true) {
    struct input_event event;
    int rc = libevdev_next_event(app->evdev, read_flag, &event);
    if (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC) {
      apply_event(app, &event);
      read_flag = rc == LIBEVDEV_READ_STATUS_SYNC
                      ? LIBEVDEV_READ_FLAG_SYNC
                      : LIBEVDEV_READ_FLAG_NORMAL;
      continue;
    }
    if (rc == -EAGAIN)
      break;
    if (rc < 0) {
      set_status(app, strerror(-rc));
      app->fd_source = 0;
      close_selected_device(app);
      app->touch_present = false;
      app->touch_pressed = false;
      memset(app->touch_active, 0, sizeof(app->touch_active));
      update_touch_label(app);
      queue_visuals(app);
      return G_SOURCE_REMOVE;
    }
  }

  (void)fd;
  return G_SOURCE_CONTINUE;
}

static gboolean on_touch_fd_event(gint fd, GIOCondition condition,
                                  gpointer user_data) {
  AppState *app = user_data;

  if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
    app->touch_fd_source = 0;
    close_touch_device(app);
    app->touch_present = false;
    app->touch_pressed = false;
    memset(app->touch_active, 0, sizeof(app->touch_active));
    update_touch_label(app);
    queue_visuals(app);
    return G_SOURCE_REMOVE;
  }

  unsigned int read_flag = LIBEVDEV_READ_FLAG_NORMAL;
  while (true) {
    struct input_event event;
    int rc = libevdev_next_event(app->touch_evdev, read_flag, &event);
    if (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC) {
      apply_touch_event(app, &event);
      read_flag = rc == LIBEVDEV_READ_STATUS_SYNC
                      ? LIBEVDEV_READ_FLAG_SYNC
                      : LIBEVDEV_READ_FLAG_NORMAL;
      continue;
    }
    if (rc == -EAGAIN)
      break;
    if (rc < 0) {
      app->touch_fd_source = 0;
      close_touch_device(app);
      app->touch_present = false;
      app->touch_pressed = false;
      memset(app->touch_active, 0, sizeof(app->touch_active));
      update_touch_label(app);
      queue_visuals(app);
      return G_SOURCE_REMOVE;
    }
  }

  (void)fd;
  return G_SOURCE_CONTINUE;
}

static void close_touch_device(AppState *app) {
  if (app->touch_fd_source) {
    g_source_remove(app->touch_fd_source);
    app->touch_fd_source = 0;
  }
  if (app->touch_evdev) {
    libevdev_free(app->touch_evdev);
    app->touch_evdev = NULL;
  }
  if (app->touch_fd >= 0) {
    close(app->touch_fd);
    app->touch_fd = -1;
  }
}

static void close_selected_device(AppState *app) {
  if (app->fd_source) {
    g_source_remove(app->fd_source);
    app->fd_source = 0;
  }
  stop_rumble_effect(app);
  if (app->evdev) {
    libevdev_free(app->evdev);
    app->evdev = NULL;
  }
  if (app->fd >= 0) {
    close(app->fd);
    app->fd = -1;
  }
  app->fd_writable = false;
  close_touch_device(app);
}

static GtkWidget *make_section_label(const char *text) {
  GtkWidget *label = gtk_label_new(text);
  gtk_widget_add_css_class(label, "section-title");
  gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
  return label;
}

static const char *axis_friendly_name(AppState *app, int code) {
  const UiLanguage lang = app ? app->lang : UI_EN;
  bool is_gamepad = app && app->evdev &&
                    (has_key(app->evdev, BTN_GAMEPAD) ||
                     has_key(app->evdev, BTN_SOUTH));

  switch (code) {
    case ABS_RX:
      if (is_gamepad) {
        return lang == UI_ES ? "ABS_RX (Stick Der X)" : (lang == UI_PT ? "ABS_RX (Stick Dir X)" : "ABS_RX (Right Stick X)");
      } else {
        return lang == UI_ES ? "ABS_RX (Rotación X)" : (lang == UI_PT ? "ABS_RX (Rotacao X)" : "ABS_RX (X Rotation)");
      }
    case ABS_RY:
      if (is_gamepad) {
        return lang == UI_ES ? "ABS_RY (Stick Der Y)" : (lang == UI_PT ? "ABS_RY (Stick Dir Y)" : "ABS_RY (Right Stick Y)");
      } else {
        return lang == UI_ES ? "ABS_RY (Rotación Y)" : (lang == UI_PT ? "ABS_RY (Rotacao Y)" : "ABS_RY (Y Rotation)");
      }
    case ABS_Z:
      if (is_gamepad) {
        return lang == UI_ES ? "ABS_Z (Gatillo Izq / LT)" : (lang == UI_PT ? "ABS_Z (Gatilho Esq / LT)" : "ABS_Z (Left Trigger / LT)");
      } else {
        return lang == UI_ES ? "ABS_Z (Eje Z)" : (lang == UI_PT ? "ABS_Z (Eixo Z)" : "ABS_Z (Z Axis)");
      }
    case ABS_RZ:
      if (is_gamepad) {
        return lang == UI_ES ? "ABS_RZ (Gatillo Der / RT)" : (lang == UI_PT ? "ABS_RZ (Gatilho Dir / RT)" : "ABS_RZ (Right Trigger / RT)");
      } else {
        return lang == UI_ES ? "ABS_RZ (Rotación Z)" : (lang == UI_PT ? "ABS_RZ (Rotacao Z)" : "ABS_RZ (Z Rotation)");
      }
    case ABS_GAS:
      return lang == UI_ES ? "ABS_GAS (Acelerador)" : (lang == UI_PT ? "ABS_GAS (Acelerador)" : "ABS_GAS (Gas)");
    case ABS_BRAKE:
      return lang == UI_ES ? "ABS_BRAKE (Freno)" : (lang == UI_PT ? "ABS_BRAKE (Travao)" : "ABS_BRAKE (Brake)");
    case ABS_RUDDER:
      return lang == UI_ES ? "ABS_RUDDER (Timón / Dial)" : (lang == UI_PT ? "ABS_RUDDER (Leme / Dial)" : "ABS_RUDDER (Rudder / Dial)");
    default:
      return libevdev_event_code_get_name(EV_ABS, code);
  }
}

static void add_axis_row_if_present(AppState *app, int code, bool *axis_added, GtkWidget *target_box) {
  if (code < 0 || code >= ABS_CNT || axis_added[code])
    return;
  if (!app->axis_present[code])
    return;

  // Skip building a row for X, Y, and Hat0 axes since they have custom visual widgets
  if (code == ABS_X || code == ABS_Y || code == ABS_HAT0X || code == ABS_HAT0Y)
    return;

  const struct input_absinfo *absinfo = libevdev_get_abs_info(app->evdev, code);
  if (!absinfo)
    return;

  AxisRow *row = g_new0(AxisRow, 1);
  row->code = code;
  row->minimum = absinfo->minimum;
  row->maximum = absinfo->maximum;
  row->flat = absinfo->flat;
  row->value = absinfo->value;

  GtkWidget *container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_add_css_class(container, "axis-row");

  GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  GtkWidget *name = gtk_label_new(axis_friendly_name(app, code));
  gtk_widget_add_css_class(name, "mono");
  gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
  gtk_widget_set_hexpand(name, TRUE);

  row->value_label = gtk_label_new("");
  gtk_label_set_width_chars(GTK_LABEL(row->value_label), 18);
  gtk_widget_add_css_class(row->value_label, "mono");
  gtk_label_set_xalign(GTK_LABEL(row->value_label), 1.0f);

  gtk_box_append(GTK_BOX(top), name);
  gtk_box_append(GTK_BOX(top), row->value_label);

  row->bar = gtk_progress_bar_new();
  gtk_widget_add_css_class(row->bar, "axis-bar");

  gtk_box_append(GTK_BOX(container), top);
  gtk_box_append(GTK_BOX(container), row->bar);
  gtk_box_append(GTK_BOX(target_box), container);

  update_axis_row(app, row);
  g_ptr_array_add(app->axis_rows, row);
  axis_added[code] = true;
}

static double get_widget_y_center(GtkWidget *widget, GtkWidget *ancestor) {
  graphene_point_t from = GRAPHENE_POINT_INIT(0.0f, 0.0f);
  graphene_point_t to = GRAPHENE_POINT_INIT(0.0f, 0.0f);
  if (gtk_widget_compute_point(widget, ancestor, &from, &to)) {
    return to.y + gtk_widget_get_height(widget) / 2.0;
  }
  return 0.0;
}

static void draw_bracket(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                         gpointer user_data) {
  (void)area;
  (void)width;

  GtkWidget *rows_box = GTK_WIDGET(user_data);
  double y_start = 0.0;
  double y_end = 0.0;

  if (rows_box) {
    GtkWidget *row1 = gtk_widget_get_first_child(rows_box);
    if (row1) {
      GtkWidget *top1 = gtk_widget_get_first_child(row1);
      if (top1) {
        GtkWidget *bar1 = gtk_widget_get_next_sibling(top1);
        if (bar1) {
          y_start = get_widget_y_center(bar1, rows_box);
        }
      }
      
      GtkWidget *row2 = gtk_widget_get_next_sibling(row1);
      if (row2) {
        GtkWidget *top2 = gtk_widget_get_first_child(row2);
        if (top2) {
          GtkWidget *bar2 = gtk_widget_get_next_sibling(top2);
          if (bar2) {
            y_end = get_widget_y_center(bar2, rows_box);
          }
        }
      }
    }
  }

  // Fallbacks if translation is not ready yet
  if (y_start <= 0.0) y_start = 24.0;
  if (y_end <= 0.0) y_end = height - 8.0;

  double x_start = 0.0;
  double x_end = 6.0;

  cairo_set_source_rgba(cr, 0.20, 0.55, 1.00, 0.85);
  cairo_set_line_width(cr, 2.0);
  cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
  cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

  cairo_move_to(cr, x_start, y_start);
  cairo_line_to(cr, x_end, y_start);
  cairo_line_to(cr, x_end, y_end);
  cairo_line_to(cr, x_start, y_end);
  cairo_stroke(cr);
}

static void rebuild_controls(AppState *app) {
  clear_box(app->axes_box);
  clear_grid(app->buttons_grid);
  g_ptr_array_set_size(app->axis_rows, 0);
  g_ptr_array_set_size(app->button_cells, 0);

  if (!app->evdev)
    return;

  // 1. Mark present axes and record their values, min, and max
  for (int code = 0; code < ABS_CNT; code++) {
    if (has_abs(app->evdev, code)) {
      const struct input_absinfo *absinfo = libevdev_get_abs_info(app->evdev, code);
      if (absinfo) {
        app->axis_present[code] = true;
        app->axis_values[code] = absinfo->value;
        app->axis_minimum[code] = absinfo->minimum;
        app->axis_maximum[code] = absinfo->maximum;
      }
    }
  }

  bool axis_added[ABS_CNT] = { false };

  // Setup combined box if both triggers ABS_Z and ABS_RZ are present and it's a gamepad
  bool is_gamepad = has_key(app->evdev, BTN_GAMEPAD) || has_key(app->evdev, BTN_SOUTH);
  bool has_z_and_rz = app->axis_present[ABS_Z] && app->axis_present[ABS_RZ] && is_gamepad;
  GtkWidget *z_rz_rows_box = NULL;

  if (has_z_and_rz) {
    GtkWidget *combined_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    z_rz_rows_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(z_rz_rows_box, TRUE);
    gtk_box_append(GTK_BOX(combined_box), z_rz_rows_box);

    GtkWidget *bracket = gtk_drawing_area_new();
    gtk_widget_set_size_request(bracket, 12, -1);
    gtk_widget_set_valign(bracket, GTK_ALIGN_FILL);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(bracket), draw_bracket, z_rz_rows_box, NULL);
    gtk_box_append(GTK_BOX(combined_box), bracket);

    gtk_box_append(GTK_BOX(app->axes_box), combined_box);
  }

  // Preferred order for showing axes in the detail list
  const int preferred_order[] = {
    ABS_RX,
    ABS_RY,
    ABS_Z,
    ABS_RZ,
    ABS_GAS,
    ABS_BRAKE,
    ABS_THROTTLE,
    ABS_RUDDER,
    ABS_WHEEL
  };
  const int preferred_count = sizeof(preferred_order) / sizeof(preferred_order[0]);

  // Add preferred axes first
  for (int i = 0; i < preferred_count; i++) {
    int code = preferred_order[i];
    GtkWidget *target = ((code == ABS_Z || code == ABS_RZ) && has_z_and_rz) ? z_rz_rows_box : app->axes_box;
    add_axis_row_if_present(app, code, axis_added, target);
  }

  // Add any remaining axes
  for (int code = 0; code < ABS_CNT; code++) {
    GtkWidget *target = ((code == ABS_Z || code == ABS_RZ) && has_z_and_rz) ? z_rz_rows_box : app->axes_box;
    add_axis_row_if_present(app, code, axis_added, target);
  }

  int button_index = 0;
  for (int code = 0; code < KEY_CNT; code++) {
    if (!is_game_button(code) || !has_key(app->evdev, code))
      continue;

    ButtonCell *cell = g_new0(ButtonCell, 1);
    cell->code = code;
    cell->number = button_index + 1;
    cell->value = libevdev_get_event_value(app->evdev, EV_KEY, code);
    app->key_values[code] = cell->value;
    app->key_present[code] = true;

    char num_str[16];
    snprintf(num_str, sizeof(num_str), "%d", button_index + 1);
    cell->cell = gtk_label_new(num_str);
    gtk_label_set_xalign(GTK_LABEL(cell->cell), 0.5f);
    gtk_label_set_yalign(GTK_LABEL(cell->cell), 0.5f);
    gtk_widget_add_css_class(cell->cell, "button-cell");
    gtk_widget_add_css_class(cell->cell, "mono");
    gtk_widget_set_size_request(cell->cell, 32, 32);
    update_button_cell(cell);
    char *tooltip = describe_button(app, code, cell->number);
    gtk_widget_set_tooltip_text(cell->cell, tooltip);
    g_free(tooltip);

    gtk_grid_attach(GTK_GRID(app->buttons_grid), cell->cell, button_index % 8,
                    button_index / 8, 1, 1);
    g_ptr_array_add(app->button_cells, cell);
    button_index++;
  }

  gtk_widget_set_visible(app->empty_label, app->axis_rows->len == 0 &&
                                               app->button_cells->len == 0);
  update_summary_labels(app);
  queue_visuals(app);
}

static void reset_device_views(AppState *app) {
  if (app->axes_box)
    clear_box(app->axes_box);
  if (app->buttons_grid)
    clear_grid(app->buttons_grid);
  if (app->axis_rows)
    g_ptr_array_set_size(app->axis_rows, 0);
  if (app->button_cells)
    g_ptr_array_set_size(app->button_cells, 0);

  if (app->device_info_label)
    gtk_label_set_text(GTK_LABEL(app->device_info_label), t(app, "select_device"));
  if (app->xy_label)
    gtk_label_set_text(GTK_LABEL(app->xy_label), t(app, "idle_xy"));
  if (app->hat_label)
    gtk_label_set_text(GTK_LABEL(app->hat_label), t(app, "idle_hat"));
  if (app->last_button_label)
    gtk_label_set_text(GTK_LABEL(app->last_button_label),
                       t(app, "last_button_none"));
  if (app->rumble_button)
    gtk_widget_set_visible(app->rumble_button, FALSE);
  if (app->empty_label)
    gtk_widget_set_visible(app->empty_label, TRUE);
  update_touch_label(app);
  queue_visuals(app);
}

static void select_device(AppState *app, guint index) {
  close_selected_device(app);
  reset_device_views(app);
  memset(app->axis_values, 0, sizeof(app->axis_values));
  memset(app->axis_minimum, 0, sizeof(app->axis_minimum));
  memset(app->axis_maximum, 0, sizeof(app->axis_maximum));
  memset(app->axis_present, 0, sizeof(app->axis_present));
  memset(app->key_values, 0, sizeof(app->key_values));
  memset(app->key_present, 0, sizeof(app->key_present));
  memset(app->touch_x, 0, sizeof(app->touch_x));
  memset(app->touch_y, 0, sizeof(app->touch_y));
  memset(app->touch_active, 0, sizeof(app->touch_active));
  app->touch_x_code = -1;
  app->touch_y_code = -1;
  app->touch_present = false;
  app->touch_pressed = false;
  app->touch_multitouch = false;
  app->touch_slot = 0;

  app->selected_index = (int)index;

  if (index >= app->devices->len) {
    set_status(app, t(app, "no_devices"));
    return;
  }

  DeviceInfo *info = g_ptr_array_index(app->devices, index);
  app->ff_effect_id = -1;
  /* Read/write only so the vibration test can work; reading is all that is
   * required, so fall back to read-only when the node is not writable. */
  app->fd = open(info->path, O_RDWR | O_NONBLOCK | O_CLOEXEC);
  app->fd_writable = app->fd >= 0;
  if (app->fd < 0)
    app->fd = open(info->path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (app->fd < 0) {
    const char *prefix = (errno == EACCES || errno == EPERM)
                             ? t(app, "permission_hint")
                             : t(app, "open_failed");
    char *message = g_strdup_printf("%s: %s", prefix, strerror(errno));
    gtk_label_set_text(GTK_LABEL(app->device_info_label), info->name);
    set_status(app, message);
    g_free(message);
    return;
  }

  int rc = libevdev_new_from_fd(app->fd, &app->evdev);
  if (rc < 0) {
    set_status(app, strerror(-rc));
    close_selected_device(app);
    return;
  }

  const char *lbl_path = "Path";
  const char *lbl_bus = "Bus";
  const char *lbl_id = "ID";
  const char *lbl_controls = "Controls";
  const char *lbl_axes = "axes";
  const char *lbl_buttons = "buttons";
  if (app->lang == UI_ES) {
    lbl_path = "Ruta";
    lbl_bus = "Conexión";
    lbl_id = "ID";
    lbl_controls = "Controles";
    lbl_axes = "ejes";
    lbl_buttons = "botones";
  } else if (app->lang == UI_PT) {
    lbl_path = "Caminho";
    lbl_bus = "Conexao";
    lbl_id = "ID";
    lbl_controls = "Controlos";
    lbl_axes = "eixos";
    lbl_buttons = "botoes";
  }

  char *escaped_name = g_markup_escape_text(info->name ? info->name : "", -1);
  char *escaped_path = g_markup_escape_text(info->path ? info->path : "", -1);
  char *escaped_bus = g_markup_escape_text(describe_bus(info->bus), -1);
  char *details = g_strdup_printf(
      "<b>%s</b>\n"
      "<span size='small' alpha='70%%'>"
      "%s: %s\n"
      "%s: %s\n"
      "%s: %04x:%04x\n"
      "%s: %d %s, %d %s"
      "</span>",
      escaped_name,
      lbl_path, escaped_path,
      lbl_bus, escaped_bus,
      lbl_id, info->vendor, info->product,
      lbl_controls, info->axis_count, lbl_axes, info->button_count, lbl_buttons);
  gtk_label_set_markup(GTK_LABEL(app->device_info_label), details);
  g_free(details);
  g_free(escaped_name);
  g_free(escaped_path);
  g_free(escaped_bus);

  rebuild_controls(app);
  if (app->rumble_button)
    gtk_widget_set_visible(app->rumble_button,
                           app->fd_writable &&
                               libevdev_has_event_code(app->evdev, EV_FF,
                                                       FF_RUMBLE));
  app->fd_source = g_unix_fd_add(
      app->fd, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, on_fd_event, app);

  if (info->touchpad_path) {
    app->touch_fd = open(info->touchpad_path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (app->touch_fd >= 0 &&
        libevdev_new_from_fd(app->touch_fd, &app->touch_evdev) == 0) {
      setup_touchpad_state(app);
      if (app->touch_present) {
        app->touch_fd_source = g_unix_fd_add(
            app->touch_fd, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
            on_touch_fd_event, app);
      } else {
        close_touch_device(app);
      }
    } else if (app->touch_fd >= 0) {
      close(app->touch_fd);
      app->touch_fd = -1;
    }
  }
  update_touch_label(app);
  set_status(app, t(app, "listening"));
}

static GtkWidget *make_device_row(DeviceInfo *info) {
  GtkWidget *row = gtk_list_box_row_new();
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_widget_set_margin_top(box, 8);
  gtk_widget_set_margin_bottom(box, 8);
  gtk_widget_set_margin_start(box, 10);
  gtk_widget_set_margin_end(box, 10);

  GtkWidget *name = gtk_label_new(info->name);
  gtk_label_set_xalign(GTK_LABEL(name), 0.0f);
  gtk_label_set_ellipsize(GTK_LABEL(name), PANGO_ELLIPSIZE_END);
  gtk_widget_set_tooltip_text(name, info->name);
  gtk_widget_add_css_class(name, "device-name");

  char *meta = g_strdup_printf("%s  •  %s%s", info->path,
                               describe_bus(info->bus),
                               info->touchpad_path ? "  •  touchpad" : "");
  GtkWidget *details = gtk_label_new(meta);
  gtk_label_set_xalign(GTK_LABEL(details), 0.0f);
  gtk_label_set_ellipsize(GTK_LABEL(details), PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class(details, "muted");
  gtk_widget_add_css_class(details, "mono");
  g_free(meta);

  gtk_box_append(GTK_BOX(box), name);
  gtk_box_append(GTK_BOX(box), details);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);
  return row;
}

static void update_main_actions(AppState *app) {
  bool has_selection = app->selected_index >= 0 &&
                       (guint)app->selected_index < app->devices->len;
  gtk_widget_set_sensitive(app->details_button, has_selection);
  if (app->devices->len == 0)
    gtk_label_set_text(GTK_LABEL(app->status_label), t(app, "no_devices"));
}

static void on_list_row_selected(GtkListBox *box, GtkListBoxRow *row,
                                 gpointer user_data) {
  AppState *app = user_data;
  (void)box;
  int index = row ? gtk_list_box_row_get_index(row) : -1;
  bool changed = index != app->selected_index;
  app->selected_index = index;
  if (changed && row && app->details_window && !app->reloading_devices)
    select_device(app, (guint)index);
  update_main_actions(app);
}

static void reload_devices(AppState *app) {
  char *previous_path = NULL;
  if (app->selected_index >= 0 &&
      (guint)app->selected_index < app->devices->len) {
    DeviceInfo *previous = g_ptr_array_index(app->devices, app->selected_index);
    previous_path = g_strdup(previous->path);
  }

  app->reloading_devices = true;
  g_clear_pointer(&app->devices, g_ptr_array_unref);
  app->devices = scan_devices();
  clear_list_box(app->device_list);

  for (guint i = 0; i < app->devices->len; i++) {
    DeviceInfo *info = g_ptr_array_index(app->devices, i);
    gtk_list_box_append(GTK_LIST_BOX(app->device_list), make_device_row(info));
  }

  if (app->devices->len == 0) {
    app->reloading_devices = false;
    app->selected_index = -1;
    if (app->details_window)
      gtk_window_destroy(GTK_WINDOW(app->details_window));
    close_selected_device(app);
    update_main_actions(app);
    g_free(previous_path);
    return;
  }

  guint selected = 0;
  if (previous_path) {
    for (guint i = 0; i < app->devices->len; i++) {
      DeviceInfo *info = g_ptr_array_index(app->devices, i);
      if (g_strcmp0(info->path, previous_path) == 0) {
        selected = i;
        break;
      }
    }
  }

  GtkListBoxRow *row =
      gtk_list_box_get_row_at_index(GTK_LIST_BOX(app->device_list), selected);
  if (row)
    gtk_list_box_select_row(GTK_LIST_BOX(app->device_list), row);
  app->selected_index = (int)selected;
  app->reloading_devices = false;

  if (app->details_window)
    select_device(app, selected);

  update_main_actions(app);
  g_free(previous_path);
}

static gboolean on_udev_event(gint fd, GIOCondition condition,
                              gpointer user_data) {
  AppState *app = user_data;
  (void)fd;

  if (condition & (G_IO_HUP | G_IO_ERR | G_IO_NVAL)) {
    app->udev_source = 0;
    return G_SOURCE_REMOVE;
  }

  bool should_reload = false;
  while (true) {
    struct udev_device *device = udev_monitor_receive_device(app->udev_monitor);
    if (!device)
      break;

    const char *action = udev_device_get_action(device);
    if (action && (strcmp(action, "add") == 0 ||
                   strcmp(action, "remove") == 0 ||
                   strcmp(action, "change") == 0)) {
      should_reload = true;
    }
    udev_device_unref(device);
  }

  if (should_reload)
    reload_devices(app);

  return G_SOURCE_CONTINUE;
}

static void start_hotplug_monitor(AppState *app) {
  app->udev = udev_new();
  if (!app->udev) {
    set_status(app, t(app, "hotplug_unavailable"));
    return;
  }

  app->udev_monitor = udev_monitor_new_from_netlink(app->udev, "udev");
  if (!app->udev_monitor) {
    set_status(app, t(app, "hotplug_unavailable"));
    return;
  }

  if (udev_monitor_filter_add_match_subsystem_devtype(app->udev_monitor,
                                                       "input", NULL) < 0 ||
      udev_monitor_enable_receiving(app->udev_monitor) < 0) {
    set_status(app, t(app, "hotplug_unavailable"));
    return;
  }

  int fd = udev_monitor_get_fd(app->udev_monitor);
  if (fd >= 0)
    app->udev_source = g_unix_fd_add(
        fd, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL, on_udev_event, app);
  else
    set_status(app, t(app, "hotplug_unavailable"));
}

static void draw_panel_background(GtkWidget *widget, cairo_t *cr, int width,
                                  int height) {
  GdkRGBA color;
  gtk_widget_get_color(widget, &color);
  double luminance =
      color.red * 0.2126 + color.green * 0.7152 + color.blue * 0.0722;
  bool dark_theme = luminance > 0.5;

  if (dark_theme)
    cairo_set_source_rgba(cr, 0.10, 0.11, 0.12, 0.72);
  else
    cairo_set_source_rgba(cr, 0.95, 0.96, 0.97, 0.92);
  cairo_rectangle(cr, 0.5, 0.5, width - 1.0, height - 1.0);
  cairo_fill(cr);

  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.24);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, 0.5, 0.5, width - 1.0, height - 1.0);
  cairo_stroke(cr);
}

static void draw_xy(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                    gpointer user_data) {
  AppState *app = user_data;
  GtkWidget *widget = GTK_WIDGET(area);

  draw_panel_background(widget, cr, width, height);

  GdkRGBA color;
  gtk_widget_get_color(widget, &color);

  double size_ref = MIN(width, height);
  double pad = size_ref * 0.08;
  if (pad < 4.0) pad = 4.0;

  double x0 = pad;
  double y0 = pad;
  double w = width - pad * 2.0;
  double h = height - pad * 2.0;
  double cx = x0 + w / 2.0;
  double cy = y0 + h / 2.0;

  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.22);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, x0, y0, w, h);
  cairo_stroke(cr);

  for (int i = 1; i < 4; i++) {
    double gx = x0 + (w * i / 4.0);
    double gy = y0 + (h * i / 4.0);
    cairo_move_to(cr, gx, y0);
    cairo_line_to(cr, gx, y0 + h);
    cairo_move_to(cr, x0, gy);
    cairo_line_to(cr, x0 + w, gy);
  }
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.11);
  cairo_stroke(cr);

  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.34);
  cairo_set_line_width(cr, 1.2);
  cairo_move_to(cr, cx, y0);
  cairo_line_to(cr, cx, y0 + h);
  cairo_move_to(cr, x0, cy);
  cairo_line_to(cr, x0 + w, cy);
  cairo_stroke(cr);

  double nx = axis_norm(app, ABS_X);
  double ny = axis_norm(app, ABS_Y);
  double px = cx + nx * (w / 2.0);
  double py = cy + ny * (h / 2.0);

  cairo_set_source_rgba(cr, 0.20, 0.55, 1.00, 0.30);
  cairo_arc(cr, px, py, size_ref * 0.08, 0, G_PI * 2.0);
  cairo_fill(cr);

  cairo_set_source_rgba(cr, 0.42, 0.70, 1.00, 0.95);
  cairo_arc(cr, px, py, size_ref * 0.03, 0, G_PI * 2.0);
  cairo_fill(cr);
}

static void draw_hat(GtkDrawingArea *area, cairo_t *cr, int width, int height,
                     gpointer user_data) {
  AppState *app = user_data;
  GtkWidget *widget = GTK_WIDGET(area);

  draw_panel_background(widget, cr, width, height);

  GdkRGBA color;
  gtk_widget_get_color(widget, &color);

  double cx = width / 2.0;
  double cy = height / 2.0;
  double r = MIN(width, height) * 0.38;
  int hx = axis_value(app, ABS_HAT0X);
  int hy = axis_value(app, ABS_HAT0Y);

  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.12);
  cairo_arc(cr, cx, cy, r, 0, G_PI * 2.0);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.34);
  cairo_set_line_width(cr, 1.2);
  cairo_stroke(cr);

  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.22);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, cx - r, cy);
  cairo_line_to(cr, cx + r, cy);
  cairo_move_to(cr, cx, cy - r);
  cairo_line_to(cr, cx, cy + r);
  cairo_stroke(cr);

  double px = cx + hx * r * 0.65;
  double py = cy + hy * r * 0.65;

  cairo_set_source_rgba(cr, 0.20, 0.55, 1.00, 0.30);
  cairo_arc(cr, px, py, r * 0.28, 0, G_PI * 2.0);
  cairo_fill(cr);

  cairo_set_source_rgba(cr, 0.42, 0.70, 1.00, 0.95);
  cairo_arc(cr, px, py, r * 0.12, 0, G_PI * 2.0);
  cairo_fill(cr);
}

static void draw_touchpad(GtkDrawingArea *area, cairo_t *cr, int width,
                          int height, gpointer user_data) {
  AppState *app = user_data;
  GtkWidget *widget = GTK_WIDGET(area);
  draw_panel_background(widget, cr, width, height);

  GdkRGBA color;
  gtk_widget_get_color(widget, &color);
  double pad = 8.0;
  double w = width - pad * 2.0;
  double h = height - pad * 2.0;
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, 0.22);
  cairo_set_line_width(cr, 1.0);
  cairo_rectangle(cr, pad, pad, w, h);
  cairo_stroke(cr);

  if (!app->touch_evdev || !app->touch_present)
    return;

  for (int slot = 0; slot < 2; slot++) {
    if (!app->touch_active[slot])
      continue;
    double nx = normalize_axis(app->touch_x[slot], app->touch_x_minimum,
                               app->touch_x_maximum);
    double ny = normalize_axis(app->touch_y[slot], app->touch_y_minimum,
                               app->touch_y_maximum);
    double px = pad + (nx + 1.0) * 0.5 * w;
    double py = pad + (ny + 1.0) * 0.5 * h;
    cairo_set_source_rgba(cr, 0.20, 0.55, 1.00,
                          slot == 0 ? 0.95 : 0.60);
    cairo_arc(cr, px, py, slot == 0 ? 8.0 : 6.0, 0, G_PI * 2.0);
    cairo_fill(cr);
  }
}

static GtkWidget *make_title_block(AppState *app) {
  GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  GtkWidget *title = gtk_label_new(t(app, "app_title"));
  GtkWidget *subtitle = gtk_label_new(t(app, "subtitle"));

  gtk_widget_add_css_class(title, "app-title");
  gtk_widget_add_css_class(subtitle, "muted");
  gtk_label_set_xalign(GTK_LABEL(title), 0.0f);
  gtk_label_set_xalign(GTK_LABEL(subtitle), 0.0f);

  gtk_box_append(GTK_BOX(box), title);
  gtk_box_append(GTK_BOX(box), subtitle);
  return box;
}

static void apply_css(void) {
  const char *css =
      "window { background: @window_bg_color; color: @window_fg_color; }"
      ".root { padding: 14px; }"
      ".topbar { padding-bottom: 10px; border-bottom: 1px solid "
      "alpha(currentColor, 0.14); }"
      ".app-title { font-size: 20px; font-weight: 700; }"
      ".muted { opacity: 0.72; }"
      ".section-title { font-weight: 700; margin-top: 8px; margin-bottom: 4px; "
      "}"
      ".mono { font-family: monospace; font-size: 12px; }"
      ".list-frame { border-radius: 6px; }"
      ".device-list row { border-bottom: 1px solid alpha(currentColor, 0.10); }"
      ".device-name { font-weight: 600; }"
      ".info-panel { padding: 10px; background: alpha(currentColor, 0.045); "
      "border: 1px solid alpha(currentColor, 0.13); border-radius: 6px; }"
      ".visual-label { font-family: monospace; font-size: 12px; opacity: 0.82; "
      "}"
      ".axis-row { padding: 6px 0; }"
      ".axis-bar trough { min-height: 7px; border-radius: 3px; }"
      ".axis-bar progress { min-height: 7px; border-radius: 3px; }"
      ".button-cell { margin: 2px; border-radius: 16px; min-width: 32px; min-height: 32px; "
      "border: 1px solid alpha(currentColor, 0.18); background: "
      "alpha(currentColor, 0.055); }"
      ".button-cell.pressed { color: white; background: #2f80ed; border-color: "
      "#69a7ff; }"
      ".status { padding-top: 8px; opacity: 0.82; }";

  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_string(provider, css);
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

static void on_details_destroy(GtkWindow *window, gpointer user_data) {
  AppState *app = user_data;
  (void)window;

  close_selected_device(app);
  app->details_window = NULL;
  app->device_info_label = NULL;
  app->xy_area = NULL;
  app->hat_area = NULL;
  app->xy_label = NULL;
  app->hat_label = NULL;
  app->touch_area = NULL;
  app->touch_label = NULL;
  app->axes_box = NULL;
  app->buttons_grid = NULL;
  app->last_button_label = NULL;
  app->rumble_button = NULL;
  app->empty_label = NULL;
}

static void show_details_window(AppState *app) {
  if (app->selected_index < 0 ||
      (guint)app->selected_index >= app->devices->len)
    return;

  if (app->details_window) {
    select_device(app, (guint)app->selected_index);
    gtk_window_present(GTK_WINDOW(app->details_window));
    return;
  }

  app->details_window = gtk_application_window_new(app->application);
  gtk_window_set_title(GTK_WINDOW(app->details_window),
                       t(app, "details_title"));
  gtk_window_set_transient_for(GTK_WINDOW(app->details_window),
                               GTK_WINDOW(app->window));
  gtk_window_set_default_size(GTK_WINDOW(app->details_window), 640, 460);
  g_signal_connect(app->details_window, "destroy",
                   G_CALLBACK(on_details_destroy), app);

  GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_add_css_class(root, "root");
  gtk_window_set_child(GTK_WINDOW(app->details_window), root);

  app->device_info_label = gtk_label_new(t(app, "select_device"));
  gtk_widget_add_css_class(app->device_info_label, "info-panel");
  gtk_label_set_xalign(GTK_LABEL(app->device_info_label), 0.0f);
  gtk_label_set_wrap(GTK_LABEL(app->device_info_label), TRUE);
  gtk_box_append(GTK_BOX(root), app->device_info_label);

  // Top row: X/Y quadrant on the left, progress bars on the right
  GtkWidget *top_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 14);
  gtk_widget_set_vexpand(top_row, TRUE);
  gtk_box_append(GTK_BOX(root), top_row);

  GtkWidget *left_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_size_request(left_col, 130, -1);
  gtk_box_append(GTK_BOX(top_row), left_col);

  // X/Y quadrant
  gtk_box_append(GTK_BOX(left_col), make_section_label(t(app, "xy")));
  app->xy_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(app->xy_area, 100, 100);
  gtk_widget_set_hexpand(app->xy_area, FALSE);
  gtk_widget_set_vexpand(app->xy_area, FALSE);
  gtk_widget_set_halign(app->xy_area, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(app->xy_area, GTK_ALIGN_CENTER);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->xy_area), draw_xy, app,
                                 NULL);
  gtk_box_append(GTK_BOX(left_col), app->xy_area);

  app->xy_label = gtk_label_new(t(app, "idle_xy"));
  gtk_label_set_width_chars(GTK_LABEL(app->xy_label), 18);
  gtk_widget_add_css_class(app->xy_label, "visual-label");
  gtk_label_set_xalign(GTK_LABEL(app->xy_label), 0.5f);
  gtk_box_append(GTK_BOX(left_col), app->xy_label);

  // Right column: scrolled axes list
  GtkWidget *right_scroll = gtk_scrolled_window_new();
  gtk_widget_set_hexpand(right_scroll, TRUE);
  gtk_widget_set_vexpand(right_scroll, TRUE);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(right_scroll),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_box_append(GTK_BOX(top_row), right_scroll);

  GtkWidget *right_col = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_widget_set_margin_end(right_col, 4);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(right_scroll), right_col);

  gtk_box_append(GTK_BOX(right_col), make_section_label(t(app, "axes")));
  app->axes_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_append(GTK_BOX(right_col), app->axes_box);

  // Bottom part: buttons, POV hat, and the touch interface where present.
  GtkWidget *bottom_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
  gtk_box_append(GTK_BOX(root), bottom_row);

  GtkWidget *buttons_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_widget_set_hexpand(buttons_panel, TRUE);
  gtk_box_append(GTK_BOX(bottom_row), buttons_panel);
  gtk_box_append(GTK_BOX(buttons_panel), make_section_label(t(app, "buttons")));

  app->buttons_grid = gtk_grid_new();
  gtk_grid_set_column_spacing(GTK_GRID(app->buttons_grid), 3);
  gtk_grid_set_row_spacing(GTK_GRID(app->buttons_grid), 3);
  gtk_widget_set_halign(app->buttons_grid, GTK_ALIGN_START);
  gtk_box_append(GTK_BOX(buttons_panel), app->buttons_grid);

  app->last_button_label = gtk_label_new(t(app, "last_button_none"));
  gtk_widget_add_css_class(app->last_button_label, "visual-label");
  gtk_label_set_xalign(GTK_LABEL(app->last_button_label), 0.0f);
  gtk_widget_set_margin_top(app->last_button_label, 6);
  gtk_box_append(GTK_BOX(buttons_panel), app->last_button_label);

  app->rumble_button = gtk_button_new_with_label(t(app, "rumble"));
  gtk_widget_set_halign(app->rumble_button, GTK_ALIGN_START);
  gtk_widget_set_margin_top(app->rumble_button, 6);
  gtk_widget_set_visible(app->rumble_button, FALSE);
  g_signal_connect(app->rumble_button, "clicked",
                   G_CALLBACK(on_rumble_clicked), app);
  gtk_box_append(GTK_BOX(buttons_panel), app->rumble_button);

  GtkWidget *pov_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_append(GTK_BOX(bottom_row), pov_panel);
  gtk_box_append(GTK_BOX(pov_panel), make_section_label(t(app, "pov")));

  app->hat_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(app->hat_area, 72, 72);
  gtk_widget_set_hexpand(app->hat_area, FALSE);
  gtk_widget_set_vexpand(app->hat_area, FALSE);
  gtk_widget_set_halign(app->hat_area, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(app->hat_area, GTK_ALIGN_CENTER);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->hat_area), draw_hat, app,
                                 NULL);
  gtk_box_append(GTK_BOX(pov_panel), app->hat_area);

  app->hat_label = gtk_label_new(t(app, "idle_hat"));
  gtk_label_set_width_chars(GTK_LABEL(app->hat_label), 12);
  gtk_widget_add_css_class(app->hat_label, "visual-label");
  gtk_label_set_xalign(GTK_LABEL(app->hat_label), 0.5f);
  gtk_box_append(GTK_BOX(pov_panel), app->hat_label);

  GtkWidget *touch_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
  gtk_box_append(GTK_BOX(bottom_row), touch_panel);
  gtk_box_append(GTK_BOX(touch_panel), make_section_label(t(app, "touchpad")));

  app->touch_area = gtk_drawing_area_new();
  gtk_widget_set_size_request(app->touch_area, 132, 72);
  gtk_widget_set_hexpand(app->touch_area, FALSE);
  gtk_widget_set_vexpand(app->touch_area, FALSE);
  gtk_widget_set_halign(app->touch_area, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(app->touch_area, GTK_ALIGN_CENTER);
  gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->touch_area),
                                 draw_touchpad, app, NULL);
  gtk_box_append(GTK_BOX(touch_panel), app->touch_area);

  app->touch_label = gtk_label_new(t(app, "no_touchpad"));
  gtk_widget_add_css_class(app->touch_label, "visual-label");
  gtk_label_set_xalign(GTK_LABEL(app->touch_label), 0.5f);
  gtk_label_set_wrap(GTK_LABEL(app->touch_label), TRUE);
  gtk_box_append(GTK_BOX(touch_panel), app->touch_label);

  app->empty_label = gtk_label_new(t(app, "no_devices"));
  gtk_widget_add_css_class(app->empty_label, "muted");
  gtk_label_set_xalign(GTK_LABEL(app->empty_label), 0.0f);
  gtk_box_append(GTK_BOX(root), app->empty_label);

  select_device(app, app->selected_index);
  gtk_window_present(GTK_WINDOW(app->details_window));
}

static void on_details_clicked(GtkButton *button, gpointer user_data) {
  (void)button;
  show_details_window(user_data);
}

static void on_row_activated(GtkListBox *box, GtkListBoxRow *row,
                             gpointer user_data) {
  (void)box;
  AppState *app = user_data;
  if (row)
    app->selected_index = gtk_list_box_row_get_index(row);
  show_details_window(app);
}

static void activate(GtkApplication *application, gpointer user_data) {
  AppState *app = user_data;
  app->application = application;

  if (app->window) {
    gtk_window_present(GTK_WINDOW(app->window));
    return;
  }

  apply_css();
  gtk_window_set_default_icon_name(APP_ID);

  app->window = gtk_application_window_new(application);
  gtk_window_set_title(GTK_WINDOW(app->window), "Miy Joy");
  gtk_window_set_default_size(GTK_WINDOW(app->window), 340, 420);

  GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_add_css_class(root, "root");
  gtk_window_set_child(GTK_WINDOW(app->window), root);

  GtkWidget *topbar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_widget_add_css_class(topbar, "topbar");
  gtk_box_append(GTK_BOX(root), topbar);

  GtkWidget *title_block = make_title_block(app);
  gtk_widget_set_hexpand(title_block, TRUE);
  gtk_box_append(GTK_BOX(topbar), title_block);

  gtk_box_append(GTK_BOX(root), make_section_label(t(app, "controllers")));

  GtkWidget *frame = gtk_frame_new(NULL);
  gtk_widget_add_css_class(frame, "list-frame");
  gtk_widget_set_vexpand(frame, TRUE);
  gtk_widget_set_size_request(frame, -1, 96);
  gtk_box_append(GTK_BOX(root), frame);

  app->device_list = gtk_list_box_new();
  gtk_list_box_set_selection_mode(GTK_LIST_BOX(app->device_list),
                                  GTK_SELECTION_SINGLE);
  gtk_widget_add_css_class(app->device_list, "device-list");
  GtkWidget *list_scroll = gtk_scrolled_window_new();
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_scroll),
                                 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_scroll), app->device_list);
  gtk_frame_set_child(GTK_FRAME(frame), list_scroll);
  g_signal_connect(app->device_list, "row-selected",
                   G_CALLBACK(on_list_row_selected), app);
  g_signal_connect(app->device_list, "row-activated",
                   G_CALLBACK(on_row_activated), app);

  app->status_label = gtk_label_new("");
  gtk_widget_add_css_class(app->status_label, "status");
  gtk_label_set_xalign(GTK_LABEL(app->status_label), 0.0f);
  gtk_box_append(GTK_BOX(root), app->status_label);

  GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
  gtk_widget_set_halign(buttons, GTK_ALIGN_END);
  gtk_box_append(GTK_BOX(root), buttons);

  app->refresh_button = gtk_button_new_with_label(t(app, "refresh"));
  gtk_box_append(GTK_BOX(buttons), app->refresh_button);
  g_signal_connect_swapped(app->refresh_button, "clicked",
                           G_CALLBACK(reload_devices), app);

  app->details_button = gtk_button_new_with_label(t(app, "details"));
  gtk_widget_add_css_class(app->details_button, "suggested-action");
  gtk_box_append(GTK_BOX(buttons), app->details_button);
  g_signal_connect(app->details_button, "clicked",
                   G_CALLBACK(on_details_clicked), app);

  GtkWidget *close_button = gtk_button_new_with_label(t(app, "close"));
  gtk_box_append(GTK_BOX(buttons), close_button);
  g_signal_connect_swapped(close_button, "clicked",
                           G_CALLBACK(gtk_window_close), app->window);

  reload_devices(app);
  start_hotplug_monitor(app);
  gtk_window_present(GTK_WINDOW(app->window));
}

static void app_state_free(AppState *app) {
  if (!app)
    return;
  close_selected_device(app);
  if (app->udev_source)
    g_source_remove(app->udev_source);
  if (app->udev_monitor)
    udev_monitor_unref(app->udev_monitor);
  if (app->udev)
    udev_unref(app->udev);
  g_clear_pointer(&app->devices, g_ptr_array_unref);
  g_clear_pointer(&app->axis_rows, g_ptr_array_unref);
  g_clear_pointer(&app->button_cells, g_ptr_array_unref);
  g_free(app);
}

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--version") == 0) {
      printf("miy-joy %s\n", APP_VERSION);
      return 0;
    }
    if (strcmp(argv[i], "--help") == 0) {
      printf("Usage: miy-joy [--list] [--version] [--help]\n");
      return 0;
    }
    if (strcmp(argv[i], "--list") == 0) {
      GPtrArray *devices = scan_devices();
      for (guint n = 0; n < devices->len; n++) {
        DeviceInfo *info = g_ptr_array_index(devices, n);
        printf("%s\t%s\t%s\t%04x:%04x\t%d axes\t%d buttons%s%s\n",
               info->path, info->name, info->bus ? info->bus : "input",
               info->vendor, info->product, info->axis_count,
               info->button_count,
               info->touchpad_path ? "\ttouchpad: " : "",
               info->touchpad_path ? info->touchpad_path : "");
      }
      if (devices->len == 0)
        printf("No controllers found\n");
      g_ptr_array_unref(devices);
      return 0;
    }
  }

  AppState *app = g_new0(AppState, 1);
  app->fd = -1;
  app->ff_effect_id = -1;
  app->touch_fd = -1;
  app->selected_index = -1;
  app->lang = detect_language();
  app->devices = g_ptr_array_new_with_free_func(device_info_free);
  app->axis_rows = g_ptr_array_new_with_free_func(g_free);
  app->button_cells = g_ptr_array_new_with_free_func(g_free);

  /* Make every window report the application id (not "miy-joy") so the shell
   * can match it to the installed launcher, and give it the launcher's icon. */
  g_set_prgname(APP_ID);
  g_set_application_name("Miy Joy");
  GtkApplication *application =
      gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(application, "activate", G_CALLBACK(activate), app);

  int status = g_application_run(G_APPLICATION(application), argc, argv);
  g_object_unref(application);
  app_state_free(app);
  return status;
}
