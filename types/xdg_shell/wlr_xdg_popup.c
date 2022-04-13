#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "types/wlr_xdg_shell.h"
#include "util/signal.h"

void handle_xdg_popup_ack_configure(
		struct wlr_xdg_popup *popup,
		struct wlr_xdg_popup_configure *configure) {
	popup->pending.geometry = configure->geometry;
	popup->pending.reactive = configure->rules.reactive;
}

struct wlr_xdg_popup_configure *send_xdg_popup_configure(
		struct wlr_xdg_popup *popup) {
	struct wlr_xdg_popup_configure *configure =
		calloc(1, sizeof(*configure));
	if (configure == NULL) {
		wl_resource_post_no_memory(popup->resource);
		return NULL;
	}
	*configure = popup->scheduled;

	if (popup->scheduled.has_reposition_token) {
		popup->scheduled.has_reposition_token = false;
		xdg_popup_send_repositioned(popup->resource,
			popup->scheduled.reposition_token);
	}

	struct wlr_box *geometry = &configure->geometry;
	xdg_popup_send_configure(popup->resource,
		geometry->x, geometry->y,
		geometry->width, geometry->height);

	return configure;
}

static void xdg_popup_grab_end(struct wlr_xdg_popup_grab *popup_grab) {
	struct wlr_xdg_popup *popup, *tmp;
	wl_list_for_each_safe(popup, tmp, &popup_grab->popups, grab_link) {
		xdg_popup_send_popup_done(popup->resource);
	}

	wlr_seat_pointer_end_grab(popup_grab->seat);
	wlr_seat_keyboard_end_grab(popup_grab->seat);
	wlr_seat_touch_end_grab(popup_grab->seat);
}

static void xdg_pointer_grab_enter(struct wlr_seat_pointer_grab *grab,
		struct wlr_surface *surface, double sx, double sy) {
	struct wlr_xdg_popup_grab *popup_grab = grab->data;
	if (wl_resource_get_client(surface->resource) == popup_grab->client) {
		wlr_seat_pointer_enter(grab->seat, surface, sx, sy);
	} else {
		wlr_seat_pointer_clear_focus(grab->seat);
	}
}

static void xdg_pointer_grab_clear_focus(struct wlr_seat_pointer_grab *grab) {
	wlr_seat_pointer_clear_focus(grab->seat);
}

static void xdg_pointer_grab_motion(struct wlr_seat_pointer_grab *grab,
		uint32_t time, double sx, double sy) {
	wlr_seat_pointer_send_motion(grab->seat, time, sx, sy);
}

static uint32_t xdg_pointer_grab_button(struct wlr_seat_pointer_grab *grab,
		uint32_t time, uint32_t button, uint32_t state) {
	uint32_t serial =
		wlr_seat_pointer_send_button(grab->seat, time, button, state);
	if (serial) {
		return serial;
	} else {
		xdg_popup_grab_end(grab->data);
		return 0;
	}
}

static void xdg_pointer_grab_axis(struct wlr_seat_pointer_grab *grab,
		uint32_t time, enum wlr_axis_orientation orientation, double value,
		int32_t value_discrete, enum wlr_axis_source source) {
	wlr_seat_pointer_send_axis(grab->seat, time, orientation, value,
		value_discrete, source);
}

static void xdg_pointer_grab_frame(struct wlr_seat_pointer_grab *grab) {
	wlr_seat_pointer_send_frame(grab->seat);
}

static void xdg_pointer_grab_cancel(struct wlr_seat_pointer_grab *grab) {
	xdg_popup_grab_end(grab->data);
}

static const struct wlr_pointer_grab_interface xdg_pointer_grab_impl = {
	.enter = xdg_pointer_grab_enter,
	.clear_focus = xdg_pointer_grab_clear_focus,
	.motion = xdg_pointer_grab_motion,
	.button = xdg_pointer_grab_button,
	.cancel = xdg_pointer_grab_cancel,
	.axis = xdg_pointer_grab_axis,
	.frame = xdg_pointer_grab_frame,
};

static void xdg_keyboard_grab_enter(struct wlr_seat_keyboard_grab *grab,
		struct wlr_surface *surface, uint32_t keycodes[], size_t num_keycodes,
		struct wlr_keyboard_modifiers *modifiers) {
	// keyboard focus should remain on the popup
}

static void xdg_keyboard_grab_clear_focus(struct wlr_seat_keyboard_grab *grab) {
	// keyboard focus should remain on the popup
}

static void xdg_keyboard_grab_key(struct wlr_seat_keyboard_grab *grab, uint32_t time,
		uint32_t key, uint32_t state) {
	wlr_seat_keyboard_send_key(grab->seat, time, key, state);
}

static void xdg_keyboard_grab_modifiers(struct wlr_seat_keyboard_grab *grab,
		struct wlr_keyboard_modifiers *modifiers) {
	wlr_seat_keyboard_send_modifiers(grab->seat, modifiers);
}

static void xdg_keyboard_grab_cancel(struct wlr_seat_keyboard_grab *grab) {
	wlr_seat_pointer_end_grab(grab->seat);
}

static const struct wlr_keyboard_grab_interface xdg_keyboard_grab_impl = {
	.enter = xdg_keyboard_grab_enter,
	.clear_focus = xdg_keyboard_grab_clear_focus,
	.key = xdg_keyboard_grab_key,
	.modifiers = xdg_keyboard_grab_modifiers,
	.cancel = xdg_keyboard_grab_cancel,
};

static uint32_t xdg_touch_grab_down(struct wlr_seat_touch_grab *grab,
		uint32_t time, struct wlr_touch_point *point) {
	struct wlr_xdg_popup_grab *popup_grab = grab->data;

	if (wl_resource_get_client(point->surface->resource) != popup_grab->client) {
		xdg_popup_grab_end(grab->data);
		return 0;
	}

	return wlr_seat_touch_send_down(grab->seat, point->surface, time,
			point->touch_id, point->sx, point->sy);
}

static void xdg_touch_grab_up(struct wlr_seat_touch_grab *grab,
		uint32_t time, struct wlr_touch_point *point) {
	wlr_seat_touch_send_up(grab->seat, time, point->touch_id);
}

static void xdg_touch_grab_motion(struct wlr_seat_touch_grab *grab,
		uint32_t time, struct wlr_touch_point *point) {
	wlr_seat_touch_send_motion(grab->seat, time, point->touch_id, point->sx,
		point->sy);
}

static void xdg_touch_grab_enter(struct wlr_seat_touch_grab *grab,
		uint32_t time, struct wlr_touch_point *point) {
}

static void xdg_touch_grab_frame(struct wlr_seat_touch_grab *grab) {
	wlr_seat_touch_send_frame(grab->seat);
}

static void xdg_touch_grab_cancel(struct wlr_seat_touch_grab *grab) {
	wlr_seat_touch_end_grab(grab->seat);
}

static const struct wlr_touch_grab_interface xdg_touch_grab_impl = {
	.down = xdg_touch_grab_down,
	.up = xdg_touch_grab_up,
	.motion = xdg_touch_grab_motion,
	.enter = xdg_touch_grab_enter,
	.frame = xdg_touch_grab_frame,
	.cancel = xdg_touch_grab_cancel
};

static void destroy_xdg_popup_grab(struct wlr_xdg_popup_grab *xdg_grab) {
	if (xdg_grab == NULL) {
		return;
	}

	wl_list_remove(&xdg_grab->seat_destroy.link);

	struct wlr_xdg_popup *popup, *tmp;
	wl_list_for_each_safe(popup, tmp, &xdg_grab->popups, grab_link) {
		destroy_xdg_surface(popup->base);
	}

	wl_list_remove(&xdg_grab->link);
	free(xdg_grab);
}

static void xdg_popup_grab_handle_seat_destroy(
		struct wl_listener *listener, void *data) {
	struct wlr_xdg_popup_grab *xdg_grab =
		wl_container_of(listener, xdg_grab, seat_destroy);
	destroy_xdg_popup_grab(xdg_grab);
}

static struct wlr_xdg_popup_grab *get_xdg_shell_popup_grab_from_seat(
		struct wlr_xdg_shell *shell, struct wlr_seat *seat) {
	struct wlr_xdg_popup_grab *xdg_grab;
	wl_list_for_each(xdg_grab, &shell->popup_grabs, link) {
		if (xdg_grab->seat == seat) {
			return xdg_grab;
		}
	}

	xdg_grab = calloc(1, sizeof(struct wlr_xdg_popup_grab));
	if (!xdg_grab) {
		return NULL;
	}

	xdg_grab->pointer_grab.data = xdg_grab;
	xdg_grab->pointer_grab.interface = &xdg_pointer_grab_impl;
	xdg_grab->keyboard_grab.data = xdg_grab;
	xdg_grab->keyboard_grab.interface = &xdg_keyboard_grab_impl;
	xdg_grab->touch_grab.data = xdg_grab;
	xdg_grab->touch_grab.interface = &xdg_touch_grab_impl;

	wl_list_init(&xdg_grab->popups);

	wl_list_insert(&shell->popup_grabs, &xdg_grab->link);
	xdg_grab->seat = seat;

	xdg_grab->seat_destroy.notify = xdg_popup_grab_handle_seat_destroy;
	wl_signal_add(&seat->events.destroy, &xdg_grab->seat_destroy);

	return xdg_grab;
}

void handle_xdg_popup_committed(struct wlr_xdg_popup *popup) {
	if (!popup->parent) {
		wl_resource_post_error(popup->base->resource,
			XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
			"xdg_popup has no parent");
		return;
	}

	if (!popup->committed) {
		wlr_xdg_surface_schedule_configure(popup->base);
		popup->committed = true;
		return;
	}

	popup->current = popup->pending;
}

static const struct xdg_popup_interface xdg_popup_implementation;

struct wlr_xdg_popup *wlr_xdg_popup_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_popup_interface,
		&xdg_popup_implementation));
	return wl_resource_get_user_data(resource);
}

static void xdg_popup_handle_grab(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *seat_resource,
		uint32_t serial) {
	struct wlr_xdg_popup *popup =
		wlr_xdg_popup_from_resource(resource);
	if (!popup) {
		return;
	}

	struct wlr_seat_client *seat_client =
		wlr_seat_client_from_resource(seat_resource);
	if (popup->committed) {
		wl_resource_post_error(popup->resource,
			XDG_POPUP_ERROR_INVALID_GRAB,
			"xdg_popup is already mapped");
		return;
	}

	struct wlr_xdg_popup_grab *popup_grab = get_xdg_shell_popup_grab_from_seat(
		popup->base->client->shell, seat_client->seat);

	if (!wl_list_empty(&popup->base->popups)) {
		wl_resource_post_error(popup->base->client->resource,
			XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP,
			"xdg_popup was not created on the topmost popup");
		return;
	}

	popup_grab->client = popup->base->client->client;
	popup->seat = seat_client->seat;

	wl_list_insert(&popup_grab->popups, &popup->grab_link);

	wlr_seat_pointer_start_grab(seat_client->seat,
		&popup_grab->pointer_grab);
	wlr_seat_keyboard_start_grab(seat_client->seat,
		&popup_grab->keyboard_grab);
	wlr_seat_touch_start_grab(seat_client->seat,
		&popup_grab->touch_grab);
}

static void xdg_popup_handle_reposition(
		struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *positioner_resource, uint32_t token) {
	struct wlr_xdg_popup *popup =
		wlr_xdg_popup_from_resource(resource);
	if (!popup) {
		return;
	}

	struct wlr_xdg_positioner *positioner =
		wlr_xdg_positioner_from_resource(positioner_resource);
	wlr_xdg_positioner_rules_get_geometry(
		&positioner->rules, &popup->scheduled.geometry);
	popup->scheduled.rules = positioner->rules;
	popup->scheduled.has_reposition_token = true;
	popup->scheduled.reposition_token = token;

	wlr_xdg_surface_schedule_configure(popup->base);

	wlr_signal_emit_safe(&popup->events.reposition, NULL);
}

static void xdg_popup_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_xdg_popup *popup =
		wlr_xdg_popup_from_resource(resource);

	if (popup && !wl_list_empty(&popup->base->popups)) {
		wl_resource_post_error(popup->base->client->resource,
			XDG_WM_BASE_ERROR_NOT_THE_TOPMOST_POPUP,
			"xdg_popup was destroyed while it was not the topmost popup");
		return;
	}

	wl_resource_destroy(resource);
}

static const struct xdg_popup_interface xdg_popup_implementation = {
	.destroy = xdg_popup_handle_destroy,
	.grab = xdg_popup_handle_grab,
	.reposition = xdg_popup_handle_reposition,
};

static void xdg_popup_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_xdg_popup *popup =
		wlr_xdg_popup_from_resource(resource);
	if (popup == NULL) {
		return;
	}
	wlr_xdg_popup_destroy(popup);
}

const struct wlr_surface_role xdg_popup_surface_role = {
	.name = "xdg_popup",
	.commit = xdg_surface_role_commit,
	.precommit = xdg_surface_role_precommit,
};

void create_xdg_popup(struct wlr_xdg_surface *surface,
		struct wlr_xdg_surface *parent,
		struct wlr_xdg_positioner *positioner, uint32_t id) {
	if (positioner->rules.size.width == 0 ||
			positioner->rules.anchor_rect.width == 0) {
		wl_resource_post_error(surface->client->resource,
			XDG_WM_BASE_ERROR_INVALID_POSITIONER,
			"positioner object is not complete");
		return;
	}

	if (surface->role != WLR_XDG_SURFACE_ROLE_NONE) {
		wl_resource_post_error(surface->resource,
			XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
			"xdg-surface has already been constructed");
		return;
	}

	if (!wlr_surface_set_role(surface->surface, &xdg_popup_surface_role,
			surface, surface->resource, XDG_WM_BASE_ERROR_ROLE)) {
		return;
	}

	assert(surface->popup == NULL);
	surface->popup = calloc(1, sizeof(struct wlr_xdg_popup));
	if (!surface->popup) {
		wl_resource_post_no_memory(surface->resource);
		return;
	}
	surface->popup->base = surface;

	surface->popup->resource = wl_resource_create(
		surface->client->client, &xdg_popup_interface,
		wl_resource_get_version(surface->resource), id);
	if (surface->popup->resource == NULL) {
		free(surface->popup);
		surface->popup = NULL;
		wl_resource_post_no_memory(surface->resource);
		return;
	}
	wl_resource_set_implementation(surface->popup->resource,
		&xdg_popup_implementation, surface->popup,
		xdg_popup_handle_resource_destroy);

	surface->role = WLR_XDG_SURFACE_ROLE_POPUP;

	wlr_xdg_positioner_rules_get_geometry(
		&positioner->rules, &surface->popup->scheduled.geometry);
	surface->popup->scheduled.rules = positioner->rules;

	wl_signal_init(&surface->popup->events.reposition);

	if (parent) {
		surface->popup->parent = parent->surface;
		wl_list_insert(&parent->popups, &surface->popup->link);
		wlr_signal_emit_safe(&parent->events.new_popup, surface->popup);
	} else {
		wl_list_init(&surface->popup->link);
	}
}

void unmap_xdg_popup(struct wlr_xdg_popup *popup) {
	if (popup->seat != NULL) {
		struct wlr_xdg_popup_grab *grab =
			get_xdg_shell_popup_grab_from_seat(
				popup->base->client->shell, popup->seat);

		wl_list_remove(&popup->grab_link);

		if (wl_list_empty(&grab->popups)) {
			if (grab->seat->pointer_state.grab == &grab->pointer_grab) {
				wlr_seat_pointer_end_grab(grab->seat);
			}
			if (grab->seat->keyboard_state.grab == &grab->keyboard_grab) {
				wlr_seat_keyboard_end_grab(grab->seat);
			}
			if (grab->seat->touch_state.grab == &grab->touch_grab) {
				wlr_seat_touch_end_grab(grab->seat);
			}

			destroy_xdg_popup_grab(grab);
		}

		popup->seat = NULL;
	}
}

void destroy_xdg_popup(struct wlr_xdg_popup *popup) {
	wl_list_remove(&popup->link);
	wl_resource_set_user_data(popup->resource, NULL);
	free(popup);
}

void wlr_xdg_popup_destroy(struct wlr_xdg_popup *popup) {
	if (popup == NULL) {
		return;
	}

	struct wlr_xdg_popup *child, *child_tmp;
	wl_list_for_each_safe(child, child_tmp, &popup->base->popups, link) {
		wlr_xdg_popup_destroy(child);
	}

	xdg_popup_send_popup_done(popup->resource);
	wl_resource_set_user_data(popup->resource, NULL);
	reset_xdg_surface(popup->base);
}

void wlr_xdg_popup_get_toplevel_coords(struct wlr_xdg_popup *popup,
		int popup_sx, int popup_sy, int *toplevel_sx, int *toplevel_sy) {
	struct wlr_surface *parent = popup->parent;
	while (wlr_surface_is_xdg_surface(parent)) {
		struct wlr_xdg_surface *xdg_surface =
			wlr_xdg_surface_from_wlr_surface(parent);

		if (xdg_surface->role == WLR_XDG_SURFACE_ROLE_POPUP) {
			popup_sx += xdg_surface->popup->current.geometry.x;
			popup_sy += xdg_surface->popup->current.geometry.y;
			parent = xdg_surface->popup->parent;
		} else {
			popup_sx += xdg_surface->current.geometry.x;
			popup_sy += xdg_surface->current.geometry.y;
			break;
		}
	}
	assert(parent);

	*toplevel_sx = popup_sx;
	*toplevel_sy = popup_sy;
}

void wlr_xdg_popup_unconstrain_from_box(struct wlr_xdg_popup *popup,
		const struct wlr_box *toplevel_space_box) {
	int toplevel_sx, toplevel_sy;
	wlr_xdg_popup_get_toplevel_coords(popup,
		0, 0, &toplevel_sx, &toplevel_sy);
	struct wlr_box popup_constraint = {
		.x = toplevel_space_box->x - toplevel_sx,
		.y = toplevel_space_box->y - toplevel_sy,
		.width = toplevel_space_box->width,
		.height = toplevel_space_box->height,
	};
	wlr_xdg_positioner_rules_unconstrain_box(&popup->scheduled.rules,
		&popup_constraint, &popup->scheduled.geometry);
	wlr_xdg_surface_schedule_configure(popup->base);
}
