// SPDX-License-Identifier: GPL-2.0-only
#include "input/cursor-shake.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "buffer.h"
#include "common/mem.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"

#define ZOOM_RAMP_MS 150
#define FRAME_MS 16

static uint32_t
now_msec(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint32_t)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
}

/* The highest output scale, so that the big pointer is sharp everywhere */
static float
max_output_scale(void)
{
	float scale = 1;
	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		if (output_is_usable(output) && output->wlr_output->scale > scale) {
			scale = output->wlr_output->scale;
		}
	}
	return scale;
}

static void
drop_big_buffer(struct cursor_shake *shake)
{
	if (shake->big_buffer) {
		wlr_buffer_drop(&shake->big_buffer->base);
		shake->big_buffer = NULL;
	}
	zfree(shake->big_buffer_name);
}

/* Loads the image of shape `name` at the big size, false if there is none */
static bool
load_big_buffer(struct cursor_shake *shake, const char *name, float scale)
{
	if (shake->big_buffer && scale == shake->big_buffer_scale
			&& !strcmp(name, shake->big_buffer_name)) {
		return true;
	}
	drop_big_buffer(shake);

	if (!shake->big_manager) {
		const char *size_env = getenv("XCURSOR_SIZE");
		uint32_t size = size_env ? atoi(size_env) : XCURSOR_SIZE;
		shake->big_manager = wlr_xcursor_manager_create(
			getenv("XCURSOR_THEME"), size * rc.shake_to_find_scale);
	}
	wlr_xcursor_manager_load(shake->big_manager, scale);
	struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(
		shake->big_manager, name, scale);
	if (!xcursor) {
		xcursor = wlr_xcursor_manager_get_xcursor(
			shake->big_manager, "default", scale);
	}
	if (!xcursor || !xcursor->image_count) {
		return false;
	}

	struct wlr_xcursor_image *image = xcursor->images[0];
	size_t len = (size_t)image->width * image->height * 4;
	void *pixels = xzalloc(len);
	memcpy(pixels, image->buffer, len);
	shake->big_buffer = buffer_create_from_data(pixels, image->width,
		image->height, image->width * 4);
	shake->big_buffer_name = xstrdup(name);
	shake->big_buffer_scale = scale;
	shake->big_hotspot_x = image->hotspot_x;
	shake->big_hotspot_y = image->hotspot_y;
	return true;
}

/* Shows the pointer enlarged according to the current progress */
static void
draw_big(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	/* Client cursors cannot be enlarged: show the default arrow */
	const char *name = shake->kind == CURSOR_REQ_XCURSOR && shake->name
		&& *shake->name ? shake->name : "default";
	float output_scale = max_output_scale();
	if (!load_big_buffer(shake, name, output_scale)) {
		return;
	}
	double factor = 1 + (rc.shake_to_find_scale - 1)
		* shake_ease(shake->progress);
	/* Buffer pixels per logical pixel */
	float buffer_scale = rc.shake_to_find_scale * output_scale / factor;
	wlr_cursor_set_buffer(seat->cursor, &shake->big_buffer->base,
		shake->big_hotspot_x / buffer_scale,
		shake->big_hotspot_y / buffer_scale, buffer_scale);
}

static void
clear_surface(struct cursor_shake *shake)
{
	if (shake->surface) {
		wl_list_remove(&shake->surface_destroy.link);
		wl_list_init(&shake->surface_destroy.link);
		shake->surface = NULL;
	}
}

static void
handle_surface_destroy(struct wl_listener *listener, void *data)
{
	struct cursor_shake *shake =
		wl_container_of(listener, shake, surface_destroy);
	/* Like wlroots: without its surface the client cursor is gone */
	clear_surface(shake);
}

static void stop_effect(struct seat *seat);

/* Shows the last requested cursor image */
static void
apply_request(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	if (shake->active) {
		draw_big(seat);
		return;
	}
	switch (shake->kind) {
	case CURSOR_REQ_XCURSOR:
		wlr_cursor_set_xcursor(seat->cursor, seat->xcursor_manager,
			shake->name);
		break;
	case CURSOR_REQ_SURFACE:
		wlr_cursor_set_surface(seat->cursor, shake->surface,
			shake->hotspot_x, shake->hotspot_y);
		break;
	case CURSOR_REQ_HIDDEN:
		wlr_cursor_unset_image(seat->cursor);
		break;
	}
}

void
cursor_image_set_xcursor(struct seat *seat, const char *name)
{
	struct cursor_shake *shake = &seat->shake;
	clear_surface(shake);
	if (shake->kind != CURSOR_REQ_XCURSOR || !shake->name
			|| strcmp(shake->name, name)) {
		free(shake->name);
		shake->name = xstrdup(name);
	}
	shake->kind = CURSOR_REQ_XCURSOR;
	apply_request(seat);
}

void
cursor_image_set_surface(struct seat *seat, struct wlr_surface *surface,
		int32_t hotspot_x, int32_t hotspot_y)
{
	struct cursor_shake *shake = &seat->shake;
	if (surface != shake->surface) {
		clear_surface(shake);
		if (surface) {
			shake->surface = surface;
			wl_list_remove(&shake->surface_destroy.link);
			wl_signal_add(&surface->events.destroy,
				&shake->surface_destroy);
		}
	}
	shake->kind = CURSOR_REQ_SURFACE;
	shake->hotspot_x = hotspot_x;
	shake->hotspot_y = hotspot_y;
	if (!surface) {
		/* The client hides the cursor */
		stop_effect(seat);
	}
	apply_request(seat);
}

static void
stop_effect(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	if (!shake->active) {
		return;
	}
	shake->active = false;
	shake->progress = 0;
	if (shake->timer) {
		wl_event_source_timer_update(shake->timer, 0);
	}
	apply_request(seat);
}

/* Whether the requested image is something that can be shown big */
static bool
has_visible_request(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	return seat->cursor_visible && (shake->kind == CURSOR_REQ_XCURSOR
		|| (shake->kind == CURSOR_REQ_SURFACE && shake->surface));
}

void
cursor_image_unset(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	clear_surface(shake);
	shake->kind = CURSOR_REQ_HIDDEN;
	stop_effect(seat);
	apply_request(seat);
}

static int
handle_timer(void *data)
{
	struct seat *seat = data;
	struct cursor_shake *shake = &seat->shake;
	uint32_t now = now_msec();
	uint32_t dt = now - shake->last_frame_msec;
	shake->last_frame_msec = now;

	uint32_t since_shake = now - shake->last_shake_msec;
	if (shake->grow && shake->progress >= 1
			&& since_shake >= (uint32_t)rc.shake_to_find_duration) {
		shake->grow = false;
	}
	shake->progress = shake_progress_step(shake->progress, shake->grow,
		dt, ZOOM_RAMP_MS);
	if (!shake->grow && shake->progress <= 0) {
		stop_effect(seat);
		return 0;
	}
	draw_big(seat);

	if (shake->grow && shake->progress >= 1) {
		/* Fully big: wait until the shaking has stopped long enough */
		uint32_t remaining = since_shake < (uint32_t)rc.shake_to_find_duration
			? rc.shake_to_find_duration - since_shake : 0;
		wl_event_source_timer_update(shake->timer,
			remaining > 0 ? remaining : 1);
	} else {
		wl_event_source_timer_update(shake->timer, FRAME_MS);
	}
	return 0;
}

void
cursor_shake_motion(struct seat *seat, double dx, double dy,
		uint32_t time_msec)
{
	struct cursor_shake *shake = &seat->shake;
	if (!rc.shake_to_find || !has_visible_request(seat)) {
		return;
	}
	if (!shake_detector_feed(&shake->detector, dx, dy, time_msec)) {
		return;
	}

	uint32_t now = now_msec();
	shake->last_shake_msec = now;
	if (shake->active && shake->grow) {
		/* Already growing or big: the timer keeps it big */
		return;
	}
	if (!shake->timer) {
		shake->timer = wl_event_loop_add_timer(server.wl_event_loop,
			handle_timer, seat);
	}
	if (!shake->active) {
		shake->active = true;
		shake->progress = 0;
		shake->last_frame_msec = now;
	}
	/* Zoom in, also from the middle of a zoom-out */
	shake->grow = true;
	wl_event_source_timer_update(shake->timer, 1);
}

void
cursor_shake_init(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	*shake = (struct cursor_shake){0};
	shake->kind = CURSOR_REQ_HIDDEN;
	shake->surface_destroy.notify = handle_surface_destroy;
	wl_list_init(&shake->surface_destroy.link);
}

void
cursor_shake_reset(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	stop_effect(seat);
	shake->detector = (struct shake_detector){0};
	drop_big_buffer(shake);
	if (shake->big_manager) {
		wlr_xcursor_manager_destroy(shake->big_manager);
		shake->big_manager = NULL;
	}
}

void
cursor_shake_finish(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	cursor_shake_reset(seat);
	if (shake->timer) {
		wl_event_source_remove(shake->timer);
		shake->timer = NULL;
	}
	clear_surface(shake);
	zfree(shake->name);
	shake->kind = CURSOR_REQ_HIDDEN;
}
