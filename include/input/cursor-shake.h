/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_CURSOR_SHAKE_H
#define LABWC_CURSOR_SHAKE_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include "common/shake.h"

struct seat;
struct wlr_surface;
struct wlr_xcursor_manager;
struct lab_data_buffer;

enum cursor_request_kind {
	CURSOR_REQ_XCURSOR,
	CURSOR_REQ_SURFACE,
	CURSOR_REQ_HIDDEN,
};

/*
 * <mouse><shakeToFind>: shaking the mouse enlarges the pointer for a moment.
 * All cursor image changes go through here, so that the last requested
 * image can be shown again when the effect ends.
 */
struct cursor_shake {
	/* The cursor image as last requested */
	enum cursor_request_kind kind;
	char *name;
	struct wlr_surface *surface;
	int32_t hotspot_x, hotspot_y;
	struct wl_listener surface_destroy;

	/* The effect */
	struct shake_detector detector;
	bool active;
	struct shake_zoom zoom;
	struct wl_event_source *timer;

	/* The enlarged pointer image */
	struct wlr_xcursor_manager *big_manager;
	struct lab_data_buffer *big_buffer;
	char *big_buffer_name;
	float big_buffer_scale;
	int32_t big_hotspot_x, big_hotspot_y;
};

/* Replacements for wlr_cursor_set_xcursor/_set_surface/_unset_image */
void cursor_image_set_xcursor(struct seat *seat, const char *name);
void cursor_image_set_surface(struct seat *seat, struct wlr_surface *surface,
	int32_t hotspot_x, int32_t hotspot_y);
void cursor_image_unset(struct seat *seat);

/* Feeds pointer motion to the shake detector */
void cursor_shake_motion(struct seat *seat, double dx, double dy,
	uint32_t time_msec);

void cursor_shake_init(struct seat *seat);
/* Ends the effect (showing the requested image again), e.g. on reconfigure */
void cursor_shake_reset(struct seat *seat);
/* Like cursor_shake_reset(), also forgets the requested image */
void cursor_shake_finish(struct seat *seat);

#endif /* LABWC_CURSOR_SHAKE_H */
