// SPDX-License-Identifier: GPL-2.0-only
#include "input/cursor-shake.h"
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include "common/mem.h"
#include "labwc.h"

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

/* Shows the last requested cursor image */
static void
apply_request(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
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
	apply_request(seat);
}

void
cursor_image_unset(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	clear_surface(shake);
	shake->kind = CURSOR_REQ_HIDDEN;
	apply_request(seat);
}

void
cursor_shake_motion(struct seat *seat, double dx, double dy,
		uint32_t time_msec)
{
	/* Nothing yet */
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
	/* Nothing yet */
}

void
cursor_shake_finish(struct seat *seat)
{
	struct cursor_shake *shake = &seat->shake;
	cursor_shake_reset(seat);
	clear_surface(shake);
	zfree(shake->name);
	shake->kind = CURSOR_REQ_HIDDEN;
}
