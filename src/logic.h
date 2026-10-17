#ifndef MIY_JOY_LOGIC_H
#define MIY_JOY_LOGIC_H

#include <stdbool.h>

static inline double normalize_axis(int value, int minimum, int maximum) {
  if (maximum <= minimum)
    return 0.0;

  double norm =
      (((double)value - (double)minimum) /
       ((double)maximum - (double)minimum)) * 2.0 - 1.0;
  if (norm < -1.0)
    return -1.0;
  if (norm > 1.0)
    return 1.0;
  return norm;
}

static inline bool is_game_button(int code) {
  if (code >= 0x100 && code < 0x110)
    return true;
  if (code >= 0x120 && code < 0x140)
    return true;
  if (code >= 0x150 && code < 0x160)
    return true;
  if (code >= 0x2c0 && code <= 0x2e7)
    return true;
  return false;
}

static inline bool is_hat_axis(int code) {
  return code >= 0x10 && code <= 0x17;
}

/* Xbox-style name for the standard Linux gamepad button codes (the layout most
 * other programs and mapping tools use as their reference). NULL if unknown. */
static inline const char *xbox_button_name(int code) {
  switch (code) {
    case 0x130: return "A";            /* BTN_SOUTH */
    case 0x131: return "B";            /* BTN_EAST */
    case 0x133: return "X";            /* BTN_NORTH */
    case 0x134: return "Y";            /* BTN_WEST */
    case 0x136: return "LB";           /* BTN_TL */
    case 0x137: return "RB";           /* BTN_TR */
    case 0x138: return "LT";           /* BTN_TL2 */
    case 0x139: return "RT";           /* BTN_TR2 */
    case 0x13a: return "View";         /* BTN_SELECT */
    case 0x13b: return "Menu";         /* BTN_START */
    case 0x13c: return "Guide";        /* BTN_MODE */
    case 0x13d: return "L3";           /* BTN_THUMBL */
    case 0x13e: return "R3";           /* BTN_THUMBR */
    default: return NULL;
  }
}

#endif
