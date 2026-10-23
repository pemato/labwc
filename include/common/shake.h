/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SHAKE_H
#define LABWC_SHAKE_H

#include <stdbool.h>
#include <stdint.h>

/* A shake: this many direction reversals of strokes this long, in this time */
#define SHAKE_WINDOW_MS 750
#define SHAKE_MIN_STROKE 40.0
#define SHAKE_REVERSALS 4

/* The zoom animation: ramp time each way and frame interval */
#define SHAKE_RAMP_MS 150
#define SHAKE_FRAME_MS 16

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

struct shake_zoom {
	double progress; /* 0 = normal size, 1 = fully big */
	bool grow;
	uint32_t last_frame_msec;
	uint32_t last_shake_msec;
};

/*
 * Advances the zoom animation to now_msec; it stays fully big until hold_ms
 * after the last shake. Returns the delay until the next tick in ms, or 0
 * when the pointer is back to its normal size.
 */
uint32_t shake_zoom_tick(struct shake_zoom *zoom, uint32_t now_msec,
	uint32_t hold_ms);

/* Smoothstep easing of a 0..1 progress */
double shake_ease(double progress);

/* Moves a 0..1 progress towards 1 (grow) or 0 by dt_ms / ramp_ms */
double shake_progress_step(double progress, bool grow, uint32_t dt_ms,
	uint32_t ramp_ms);

#endif /* LABWC_SHAKE_H */
