/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SHAKE_H
#define LABWC_SHAKE_H

#include <stdbool.h>
#include <stdint.h>

/* A shake: this many direction reversals of strokes this long, in this time */
#define SHAKE_WINDOW_MS 500
#define SHAKE_MIN_STROKE 30.0
#define SHAKE_REVERSALS 4

struct shake_axis {
	int sign;       /* direction of the current stroke: -1, 0 or 1 */
	double stroke;  /* length of the current stroke */
	uint32_t reversals[SHAKE_REVERSALS]; /* times, oldest first */
	int count;
};

struct shake_detector {
	struct shake_axis x, y;
	uint32_t last_msec;
};

/* Feeds one motion delta; returns true while the motion counts as shaking */
bool shake_detector_feed(struct shake_detector *detector, double dx,
	double dy, uint32_t time_msec);

/* Smoothstep easing of a 0..1 progress */
double shake_ease(double progress);

/* Moves a 0..1 progress towards 1 (grow) or 0 by dt_ms / ramp_ms */
double shake_progress_step(double progress, bool grow, uint32_t dt_ms,
	uint32_t ramp_ms);

#endif /* LABWC_SHAKE_H */
