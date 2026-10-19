// SPDX-License-Identifier: GPL-2.0-only
#include "foreign-toplevel/lab-workspace.h"
#include <assert.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_ext_workspace_v1.h>
#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <wlr/util/log.h>
#include "common/mem.h"
#include "lab-toplevel-workspace-v1-protocol.h"
#include "view.h"
#include "workspaces.h"

#define LAB_TOPLEVEL_WORKSPACE_VERSION 1

struct lab_toplevel_workspace_manager {
	struct wl_global *global;
	struct wl_listener display_destroy;
};

struct lab_toplevel_workspace {
	struct wl_resource *resource;
	struct view *view;             /* NULL once inert */
	struct wl_listener on_workspace;
	struct wl_listener on_view_destroy;
	struct wl_listener on_handle_destroy;
};

static void
send_enter(struct lab_toplevel_workspace *tw, struct workspace *workspace)
{
	if (!workspace || !workspace->ext_workspace
			|| !workspace->ext_workspace->id) {
		return;
	}
	lab_toplevel_workspace_v1_send_workspace_enter(tw->resource,
		workspace->ext_workspace->id);
}

static void
detach(struct lab_toplevel_workspace *tw)
{
	wl_list_remove(&tw->on_workspace.link);
	wl_list_remove(&tw->on_view_destroy.link);
	wl_list_remove(&tw->on_handle_destroy.link);
	tw->view = NULL;
}

static void
handle_workspace(struct wl_listener *listener, void *data)
{
	struct lab_toplevel_workspace *tw =
		wl_container_of(listener, tw, on_workspace);
	struct workspace *old = data;
	assert(tw->view);

	if (old && old->ext_workspace && old->ext_workspace->id) {
		lab_toplevel_workspace_v1_send_workspace_leave(tw->resource,
			old->ext_workspace->id);
	}
	wlr_log(WLR_DEBUG, "toplevel workspace: '%s' -> '%s'",
		old ? old->name : "(none)",
		tw->view->workspace ? tw->view->workspace->name : "(none)");
	send_enter(tw, tw->view->workspace);
}

static void
handle_view_destroy(struct wl_listener *listener, void *data)
{
	struct lab_toplevel_workspace *tw =
		wl_container_of(listener, tw, on_view_destroy);
	detach(tw);
}

static void
handle_handle_destroy(struct wl_listener *listener, void *data)
{
	/* Toplevel closed (unmap): become inert */
	struct lab_toplevel_workspace *tw =
		wl_container_of(listener, tw, on_handle_destroy);
	detach(tw);
}

static void
toplevel_workspace_destroy_request(struct wl_client *client,
		struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static const struct lab_toplevel_workspace_v1_interface toplevel_workspace_impl = {
	.destroy = toplevel_workspace_destroy_request,
};

static void
toplevel_workspace_resource_destroy(struct wl_resource *resource)
{
	struct lab_toplevel_workspace *tw = wl_resource_get_user_data(resource);
	if (tw->view) {
		detach(tw);
	}
	free(tw);
}

static void
manager_destroy_request(struct wl_client *client, struct wl_resource *resource)
{
	wl_resource_destroy(resource);
}

static void
manager_get_toplevel_workspace(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *toplevel_resource)
{
	struct wlr_foreign_toplevel_handle_v1 *handle =
		wl_resource_get_user_data(toplevel_resource);
	/* handle is NULL if the toplevel was already closed (inert object) */
	struct view *view = handle ? handle->data : NULL;

	struct lab_toplevel_workspace *tw = znew(*tw);
	tw->resource = wl_resource_create(client,
		&lab_toplevel_workspace_v1_interface,
		wl_resource_get_version(manager_resource), id);
	if (!tw->resource) {
		free(tw);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(tw->resource, &toplevel_workspace_impl,
		tw, toplevel_workspace_resource_destroy);

	if (!view) {
		/* Inert object: no listeners, no events */
		return;
	}

	tw->view = view;
	tw->on_workspace.notify = handle_workspace;
	wl_signal_add(&view->events.workspace, &tw->on_workspace);
	tw->on_view_destroy.notify = handle_view_destroy;
	wl_signal_add(&view->events.destroy, &tw->on_view_destroy);
	tw->on_handle_destroy.notify = handle_handle_destroy;
	wl_signal_add(&handle->events.destroy, &tw->on_handle_destroy);

	send_enter(tw, view->workspace);
}

static const struct lab_toplevel_workspace_manager_v1_interface manager_impl = {
	.destroy = manager_destroy_request,
	.get_toplevel_workspace = manager_get_toplevel_workspace,
};

static void
manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	struct wl_resource *resource = wl_resource_create(client,
		&lab_toplevel_workspace_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

static void
handle_display_destroy(struct wl_listener *listener, void *data)
{
	struct lab_toplevel_workspace_manager *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct lab_toplevel_workspace_manager *
lab_toplevel_workspace_manager_create(struct wl_display *display)
{
	struct lab_toplevel_workspace_manager *manager = znew(*manager);
	manager->global = wl_global_create(display,
		&lab_toplevel_workspace_manager_v1_interface,
		LAB_TOPLEVEL_WORKSPACE_VERSION, NULL, manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}
	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);
	return manager;
}
