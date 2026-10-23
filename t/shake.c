// SPDX-License-Identifier: GPL-2.0-only
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <cmocka.h>
#include "common/shake.h"

/* Feeds `steps` equal deltas 10 ms apart; returns the last result */
static bool
stroke_xy(struct shake_detector *d, double dx_total, double dy_total,
		int steps, uint32_t *t)
{
	bool hit = false;
	for (int i = 0; i < steps; i++) {
		*t += 10;
		hit = shake_detector_feed(d, dx_total / steps, dy_total / steps, *t);
	}
	return hit;
}

static bool
stroke(struct shake_detector *d, double dx_total, int steps, uint32_t *t)
{
	return stroke_xy(d, dx_total, 0, steps, t);
}

static bool
stroke_y(struct shake_detector *d, double dy_total, int steps, uint32_t *t)
{
	return stroke_xy(d, 0, dy_total, steps, t);
}

static void
test_shake_detected(void **state)
{
	/* 5 strokes of 60 px, 50 ms each -> 4 reversals in 250 ms */
	struct shake_detector d = {0};
	uint32_t t = 1000;
	bool hit = false;
	for (int i = 0; i < 5; i++) {
		hit = stroke(&d, i % 2 ? -60 : 60, 5, &t);
	}
	assert_true(hit);
}

static void
test_moderate_shake_detected(void **state)
{
	/* 5 strokes of 60 px, 150 ms each (about 3 Hz) */
	struct shake_detector d = {0};
	uint32_t t = 1000;
	bool hit = false;
	for (int i = 0; i < 5; i++) {
		hit = stroke(&d, i % 2 ? -60 : 60, 15, &t);
	}
	assert_true(hit);
}

static void
test_straight_fast_move_no_shake(void **state)
{
	struct shake_detector d = {0};
	uint32_t t = 1000;
	assert_false(stroke(&d, 2000, 20, &t));
}

static void
test_small_jitter_no_shake(void **state)
{
	/* 10 px wiggles never reach SHAKE_MIN_STROKE */
	struct shake_detector d = {0};
	uint32_t t = 1000;
	bool hit = false;
	for (int i = 0; i < 12; i++) {
		hit |= stroke(&d, i % 2 ? -10 : 10, 2, &t);
	}
	assert_false(hit);
}

static void
test_slow_reversals_no_shake(void **state)
{
	/* strokes 300 ms apart: reversals fall out of the 750 ms window */
	struct shake_detector d = {0};
	uint32_t t = 1000;
	bool hit = false;
	for (int i = 0; i < 6; i++) {
		hit |= stroke(&d, i % 2 ? -60 : 60, 2, &t);
		t += 300;
	}
	assert_false(hit);
}

static void
test_vertical_shake_detected(void **state)
{
	struct shake_detector d = {0};
	uint32_t t = 1000;
	bool hit = false;
	for (int i = 0; i < 5; i++) {
		hit = stroke_y(&d, i % 2 ? -60 : 60, 5, &t);
	}
	assert_true(hit);
}

static void
test_ease_and_progress(void **state)
{
	assert_true(shake_ease(0) == 0);
	assert_true(shake_ease(1) == 1);
	assert_true(shake_ease(0.5) == 0.5);
	assert_true(shake_ease(0.25) < 0.25); /* eased start */
	assert_true(shake_progress_step(0, true, 75, 150) == 0.5);
	assert_true(shake_progress_step(0.9, true, 75, 150) == 1);
	assert_true(shake_progress_step(0.2, false, 75, 150) == 0);
}

/* Ticks the animation at the delays it asks for until it is over */
static void
test_zoom_out_animates(void **state)
{
	struct shake_zoom zoom = {
		.grow = true, .last_frame_msec = 1000, .last_shake_msec = 1000,
	};
	uint32_t t = 1000;
	uint32_t delay = 1;
	/* Zoom in, then hold */
	while (zoom.progress < 1) {
		t += delay;
		delay = shake_zoom_tick(&zoom, t, 2000);
	}
	assert_true(t - 1000 <= SHAKE_RAMP_MS + SHAKE_FRAME_MS);
	/* The hold ends 2000 ms after the shake: the zoom-out starts there */
	t += delay;
	assert_int_equal(t, 3000);
	delay = shake_zoom_tick(&zoom, t, 2000);
	assert_true(zoom.progress > 0.8);
	/* ...and takes about SHAKE_RAMP_MS */
	int frames = 0;
	while (delay) {
		t += delay;
		delay = shake_zoom_tick(&zoom, t, 2000);
		frames++;
	}
	assert_true(frames >= SHAKE_RAMP_MS / SHAKE_FRAME_MS - 1);
	assert_true(t - 3000 <= SHAKE_RAMP_MS + SHAKE_FRAME_MS);
}

static void
test_buffer_scale_uses_real_sizes(void **state)
{
	/* Adwaita: 24 px normal, 64 px nearest to the 72 px asked for */
	float scale = shake_buffer_scale(64, 24, 1, 1);
	assert_true(64 / scale > 23.99 && 64 / scale < 24.01);
	scale = shake_buffer_scale(64, 24, 1, 3);
	assert_true(64 / scale > 71.99 && 64 / scale < 72.01);
	/* On a 2x output the images are twice as wide, the logical size not */
	scale = shake_buffer_scale(128, 48, 2, 3);
	assert_true(128 / scale > 71.99 && 128 / scale < 72.01);
}

int main(int argc, char **argv)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_shake_detected),
		cmocka_unit_test(test_moderate_shake_detected),
		cmocka_unit_test(test_straight_fast_move_no_shake),
		cmocka_unit_test(test_small_jitter_no_shake),
		cmocka_unit_test(test_slow_reversals_no_shake),
		cmocka_unit_test(test_vertical_shake_detected),
		cmocka_unit_test(test_ease_and_progress),
		cmocka_unit_test(test_zoom_out_animates),
		cmocka_unit_test(test_buffer_scale_uses_real_sizes),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
