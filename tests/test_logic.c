#include <glib.h>
#include <limits.h>

#include "../src/logic.h"

static void test_normalize_axis(void) {
  g_assert_cmpfloat_with_epsilon(normalize_axis(-32768, -32768, 32767),
                                 -1.0, 1e-12);
  g_assert_cmpfloat_with_epsilon(normalize_axis(32767, -32768, 32767),
                                 1.0, 1e-12);
  g_assert_cmpfloat_with_epsilon(normalize_axis(0, -100, 100), 0.0, 1e-12);
  g_assert_cmpfloat_with_epsilon(normalize_axis(10, 10, 10), 0.0, 1e-12);
  g_assert_cmpfloat_with_epsilon(normalize_axis(0, 100, -100), 0.0, 1e-12);

  double low = normalize_axis(INT_MIN, INT_MIN, INT_MAX);
  double high = normalize_axis(INT_MAX, INT_MIN, INT_MAX);
  g_assert_cmpfloat(low, >=, -1.0);
  g_assert_cmpfloat(low, <=, 1.0);
  g_assert_cmpfloat(high, >=, -1.0);
  g_assert_cmpfloat(high, <=, 1.0);
}

static void test_code_classification(void) {
  g_assert_true(is_game_button(0x100));
  g_assert_true(is_game_button(0x13f));
  g_assert_true(is_game_button(0x2e7));
  g_assert_false(is_game_button(0x11f));
  g_assert_false(is_game_button(0x300));

  g_assert_true(is_hat_axis(0x10));
  g_assert_true(is_hat_axis(0x17));
  g_assert_false(is_hat_axis(0x0f));
  g_assert_false(is_hat_axis(0x18));
}

static void test_xbox_button_name(void) {
  g_assert_cmpstr(xbox_button_name(0x130), ==, "A");
  g_assert_cmpstr(xbox_button_name(0x133), ==, "X");
  g_assert_cmpstr(xbox_button_name(0x13e), ==, "R3");
  g_assert_null(xbox_button_name(0x132));
  g_assert_null(xbox_button_name(0x2c0));
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/logic/normalize-axis", test_normalize_axis);
  g_test_add_func("/logic/code-classification", test_code_classification);
  g_test_add_func("/logic/xbox-button-name", test_xbox_button_name);
  return g_test_run();
}
