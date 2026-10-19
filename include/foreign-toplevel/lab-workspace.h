/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_FOREIGN_TOPLEVEL_LAB_WORKSPACE_H
#define LABWC_FOREIGN_TOPLEVEL_LAB_WORKSPACE_H

struct wl_display;

/*
 * Server side of the private lab-toplevel-workspace-v1 protocol.
 * The manager lives until the display is destroyed.
 */
struct lab_toplevel_workspace_manager;

struct lab_toplevel_workspace_manager *lab_toplevel_workspace_manager_create(
	struct wl_display *display);

#endif /* LABWC_FOREIGN_TOPLEVEL_LAB_WORKSPACE_H */
