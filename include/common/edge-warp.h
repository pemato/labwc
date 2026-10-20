/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_EDGE_WARP_GEOMETRY_H
#define LABWC_EDGE_WARP_GEOMETRY_H

#include "common/edge.h"

/*
 * Returns the layout x-coordinate the cursor is warped to after it pushed
 * against the LAB_EDGE_LEFT or LAB_EDGE_RIGHT edge of an area spanning
 * area_x..area_x + area_width: just outside the edge range on the opposite
 * side, so that the edge-snapping check does not trigger there.
 */
int edge_warp_cursor_x(enum lab_edge crossed, int area_x, int area_width,
	int range);

#endif /* LABWC_EDGE_WARP_GEOMETRY_H */
