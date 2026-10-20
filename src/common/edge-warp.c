// SPDX-License-Identifier: GPL-2.0-only
#include "common/edge-warp.h"

int
edge_warp_cursor_x(enum lab_edge crossed, int area_x, int area_width,
		int range)
{
	if (crossed == LAB_EDGE_LEFT) {
		return area_x + area_width - range - 1;
	}
	return area_x + range + 1;
}
