// SPDX-License-Identifier: GPL-2.0-only
#include "edge-warp.h"
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include "common/edge-warp.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"
#include "overlay.h"
#include "regions.h"
#include "view.h"
#include "workspaces.h"

/*
 * Returns the desktop to switch to when the cursor rests at the edge it is
 * at now, or NULL if it is not at the plain left or right edge of the layout
 * or there is no desktop in that direction.
 */
static struct workspace *
target_from_cursor(struct seat *seat, struct output **output,
		enum lab_edge *edge)
{
	enum lab_edge edge1, edge2;
	if (regions_should_snap()
			|| !edge_from_cursor(seat, output, &edge1, &edge2)
			|| edge2 != LAB_EDGE_NONE
			|| (edge1 != LAB_EDGE_LEFT && edge1 != LAB_EDGE_RIGHT)) {
		return NULL;
	}
	/* Edges between outputs keep moving the window to the other output */
	enum wlr_direction direction = edge1 == LAB_EDGE_LEFT
		? WLR_DIRECTION_LEFT : WLR_DIRECTION_RIGHT;
	if (wlr_output_layout_adjacent_output(server.output_layout, direction,
			(*output)->wlr_output, seat->cursor->x, seat->cursor->y)) {
		return NULL;
	}
	*edge = edge1;
	return workspaces_find(server.workspaces.current,
		edge1 == LAB_EDGE_LEFT ? "left" : "right", /*wrap*/ false);
}

static int
handle_timeout(void *data)
{
	struct seat *seat = data;
	struct view *view = server.grabbed_view;
	struct output *output;
	enum lab_edge edge;
	struct workspace *target = NULL;
	if (view && server.input_mode == LAB_INPUT_STATE_MOVE) {
		target = target_from_cursor(seat, &output, &edge);
	}
	if (!target || edge != seat->edge_warp.edge
			|| output != seat->edge_warp.output
			|| !output_is_usable(output)) {
		edge_warp_finish(seat);
		return 0;
	}

	struct wlr_box area = output_usable_area_in_layout_coords(output);
	int new_x = edge_warp_cursor_x(edge, area.x, area.width,
		rc.snap_edge_range_outer);
	int dx = new_x - (int)seat->cursor->x;

	overlay_finish(seat);
	workspaces_switch_to_while_moving(target);

	/*
	 * The window keeps its offset to the cursor: interactive moves place
	 * it at the grab position plus the cursor motion, which includes dx.
	 */
	wlr_cursor_warp(seat->cursor, NULL, new_x, seat->cursor->y);
	view_move(view, view->pending.x + dx, view->pending.y);

	/* Wait for the cursor to reach an edge again */
	seat->edge_warp.edge = LAB_EDGE_NONE;
	seat->edge_warp.output = NULL;
	return 0;
}

void
edge_warp_update(struct seat *seat)
{
	if (!rc.screen_edge_warp_time) {
		return;
	}
	struct output *output;
	enum lab_edge edge;
	if (!target_from_cursor(seat, &output, &edge)) {
		edge_warp_finish(seat);
		return;
	}
	if (seat->edge_warp.edge == edge && seat->edge_warp.output == output) {
		/* Already waiting at this edge */
		return;
	}
	seat->edge_warp.edge = edge;
	seat->edge_warp.output = output;
	if (!seat->edge_warp.timer) {
		seat->edge_warp.timer = wl_event_loop_add_timer(
			server.wl_event_loop, handle_timeout, seat);
	}
	wl_event_source_timer_update(seat->edge_warp.timer,
		rc.screen_edge_warp_time);
}

void
edge_warp_finish(struct seat *seat)
{
	if (seat->edge_warp.timer) {
		wl_event_source_remove(seat->edge_warp.timer);
	}
	seat->edge_warp = (struct edge_warp){0};
}
