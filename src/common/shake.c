// SPDX-License-Identifier: GPL-2.0-only
#include "common/shake.h"
#include <string.h>

static void
drop_old_reversals(struct shake_axis *axis, uint32_t time_msec)
{
	int keep = 0;
	for (int i = 0; i < axis->count; i++) {
		if (time_msec - axis->reversals[i] <= SHAKE_WINDOW_MS) {
			axis->reversals[keep++] = axis->reversals[i];
		}
	}
	axis->count = keep;
}

static bool
feed_axis(struct shake_axis *axis, double delta, uint32_t time_msec)
{
	drop_old_reversals(axis, time_msec);
	if (delta == 0) {
		return axis->count >= SHAKE_REVERSALS;
	}

	int sign = delta > 0 ? 1 : -1;
	if (sign != axis->sign) {
		if (axis->sign && axis->stroke >= SHAKE_MIN_STROKE) {
			if (axis->count == SHAKE_REVERSALS) {
				/* Window full: forget the oldest */
				memmove(&axis->reversals[0], &axis->reversals[1],
					(SHAKE_REVERSALS - 1) * sizeof(uint32_t));
				axis->count--;
			}
			axis->reversals[axis->count++] = time_msec;
		}
		axis->sign = sign;
		axis->stroke = 0;
	}
	axis->stroke += delta > 0 ? delta : -delta;
	return axis->count >= SHAKE_REVERSALS;
}

bool
shake_detector_feed(struct shake_detector *detector, double dx, double dy,
		uint32_t time_msec)
{
	if (detector->last_msec
			&& time_msec - detector->last_msec > SHAKE_WINDOW_MS) {
		/* The motion paused: start over */
		*detector = (struct shake_detector){0};
	}
	detector->last_msec = time_msec;

	bool x = feed_axis(&detector->x, dx, time_msec);
	bool y = feed_axis(&detector->y, dy, time_msec);
	return x || y;
}

double
shake_ease(double progress)
{
	return progress * progress * (3 - 2 * progress);
}

double
shake_progress_step(double progress, bool grow, uint32_t dt_ms,
		uint32_t ramp_ms)
{
	double step = ramp_ms ? (double)dt_ms / ramp_ms : 1;
	progress += grow ? step : -step;
	if (progress < 0) {
		return 0;
	}
	if (progress > 1) {
		return 1;
	}
	return progress;
}

uint32_t
shake_zoom_tick(struct shake_zoom *zoom, uint32_t now_msec, uint32_t hold_ms)
{
	uint32_t dt = now_msec - zoom->last_frame_msec;
	zoom->last_frame_msec = now_msec;

	uint32_t since_shake = now_msec - zoom->last_shake_msec;
	if (zoom->grow && zoom->progress >= 1 && since_shake >= hold_ms) {
		/* The hold is over: the zoom-out starts now, not at the last tick */
		zoom->grow = false;
		dt = 0;
	}
	zoom->progress = shake_progress_step(zoom->progress, zoom->grow, dt,
		SHAKE_RAMP_MS);
	if (!zoom->grow && zoom->progress <= 0) {
		return 0;
	}
	if (zoom->grow && zoom->progress >= 1) {
		/* Fully big: wait until the shaking has stopped long enough */
		uint32_t remaining = since_shake < hold_ms
			? hold_ms - since_shake : 0;
		return remaining > 0 ? remaining : 1;
	}
	return SHAKE_FRAME_MS;
}

float
shake_buffer_scale(uint32_t big_width, uint32_t normal_width,
		float output_scale, double factor)
{
	if (!big_width || !normal_width || factor <= 0) {
		return output_scale;
	}
	return big_width * output_scale / (factor * normal_width);
}
