// SPDX-License-Identifier: GPL-2.0-only
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <cmocka.h>
#include "common/edge-warp.h"

static void
test_after_right_edge(void **state)
{
	/* area 0..1280, range 10: lands at x = 11 (left = 11, not < 10) */
	assert_int_equal(edge_warp_cursor_x(LAB_EDGE_RIGHT, 0, 1280, 10), 11);
	/* second output at x = 1920 */
	assert_int_equal(edge_warp_cursor_x(LAB_EDGE_RIGHT, 1920, 1280, 10), 1931);
}

static void
test_after_left_edge(void **state)
{
	/* right = 1280 - 1269 = 11, not < 10 */
	assert_int_equal(edge_warp_cursor_x(LAB_EDGE_LEFT, 0, 1280, 10), 1269);
	assert_int_equal(edge_warp_cursor_x(LAB_EDGE_LEFT, 1920, 1280, 10), 3189);
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_after_right_edge),
		cmocka_unit_test(test_after_left_edge),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
