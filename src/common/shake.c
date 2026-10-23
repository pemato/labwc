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
