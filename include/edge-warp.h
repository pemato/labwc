/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_EDGE_WARP_H
#define LABWC_EDGE_WARP_H

#include "common/edge.h"

struct output;
struct seat;
struct wl_event_source;

/* <mouse><screenEdgeWarpTime>: wait at an output edge during a move */
struct edge_warp {
	/* The edge the cursor rests at, LAB_EDGE_NONE if none */
	enum lab_edge edge;
	struct output *output;
	struct wl_event_source *timer;
};

/*
 * Starts or cancels the wait for a desktop switch when the grabbed window
 * is moved to (or away from) the left or right edge of the layout.
 */
void edge_warp_update(struct seat *seat);

/* Cancels a pending desktop switch */
void edge_warp_finish(struct seat *seat);

#endif /* LABWC_EDGE_WARP_H */
