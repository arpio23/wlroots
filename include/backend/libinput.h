#ifndef BACKEND_LIBINPUT_H
#define BACKEND_LIBINPUT_H

#include <libinput.h>
#include <wayland-server-core.h>
#include <wlr/backend/interface.h>
#include <wlr/backend/libinput.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/interfaces/wlr_switch.h>
#include <wlr/interfaces/wlr_tablet_pad.h>
#include <wlr/interfaces/wlr_tablet_tool.h>
#include <wlr/interfaces/wlr_touch.h>
#include <wlr/types/wlr_input_device.h>

struct wlr_libinput_backend {
	struct wlr_backend backend;

	struct wlr_session *session;
	struct wl_display *display;

	struct libinput *libinput_context;
	struct wl_event_source *input_event;

	struct wl_listener display_destroy;
	struct wl_listener session_destroy;
	struct wl_listener session_signal;

	struct wl_array wlr_device_lists; // struct wl_list *
	struct wl_list devices; // wlr_libinput_device::link
};

struct wlr_libinput_input_device {
	struct wlr_input_device wlr_input_device;
	struct libinput_device *handle;

	struct wlr_keyboard keyboard;
	struct wlr_pointer pointer;
	struct wlr_switch switch_device;
	struct wlr_touch touch;

	struct wl_list link;
};

uint32_t usec_to_msec(uint64_t usec);

void handle_libinput_event(struct wlr_libinput_backend *state,
		struct libinput_event *event);

struct wlr_input_device *get_appropriate_device(
		enum wlr_input_device_type desired_type,
		struct libinput_device *device);

void destroy_libinput_input_device(struct wlr_libinput_input_device *dev);

extern const struct wlr_keyboard_impl libinput_keyboard_impl;
extern const struct wlr_pointer_impl libinput_pointer_impl;
extern const struct wlr_switch_impl libinput_switch_impl;
extern const struct wlr_tablet_impl libinput_tablet_impl;
extern const struct wlr_tablet_pad_impl libinput_tablet_pad_impl;
extern const struct wlr_touch_impl libinput_touch_impl;

void init_device_keyboard(struct wlr_libinput_input_device *dev);
struct wlr_libinput_input_device *device_from_keyboard(struct wlr_keyboard *kb);
void handle_keyboard_key(struct libinput_event *event, struct wlr_keyboard *kb);

void init_device_pointer(struct wlr_libinput_input_device *dev);
struct wlr_libinput_input_device *device_from_pointer(struct wlr_pointer *kb);
void handle_pointer_motion(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_motion_abs(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_button(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_axis(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_swipe_begin(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_swipe_update(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_swipe_end(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_pinch_begin(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_pinch_update(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_pinch_end(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_hold_begin(struct libinput_event *event,
	struct wlr_pointer *pointer);
void handle_pointer_hold_end(struct libinput_event *event,
	struct wlr_pointer *pointer);

void init_device_switch(struct wlr_libinput_input_device *dev);
struct wlr_libinput_input_device *device_from_switch(
	struct wlr_switch *switch_device);
void handle_switch_toggle(struct libinput_event *event,
	struct wlr_switch *switch_device);

void init_device_touch(struct wlr_libinput_input_device *dev);
struct wlr_libinput_input_device *device_from_touch(
	struct wlr_touch *touch);
void handle_touch_down(struct libinput_event *event,
	struct wlr_touch *touch);
void handle_touch_up(struct libinput_event *event,
	struct wlr_touch *touch);
void handle_touch_motion(struct libinput_event *event,
	struct wlr_touch *touch);
void handle_touch_cancel(struct libinput_event *event,
	struct wlr_touch *touch);
void handle_touch_frame(struct libinput_event *event,
	struct wlr_touch *touch);

struct wlr_tablet *create_libinput_tablet(
		struct libinput_device *device);
void handle_tablet_tool_axis(struct libinput_event *event,
		struct libinput_device *device);
void handle_tablet_tool_proximity(struct libinput_event *event,
		struct libinput_device *device);
void handle_tablet_tool_tip(struct libinput_event *event,
		struct libinput_device *device);
void handle_tablet_tool_button(struct libinput_event *event,
		struct libinput_device *device);

struct wlr_tablet_pad *create_libinput_tablet_pad(
		struct libinput_device *device);
void handle_tablet_pad_button(struct libinput_event *event,
		struct libinput_device *device);
void handle_tablet_pad_ring(struct libinput_event *event,
		struct libinput_device *device);
void handle_tablet_pad_strip(struct libinput_event *event,
		struct libinput_device *device);

#endif
