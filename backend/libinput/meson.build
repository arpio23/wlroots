wlr_files += files(
	'backend.c',
	'events.c',
	'keyboard.c',
	'pointer.c',
	'switch.c',
	'tablet_pad.c',
	'tablet_tool.c',
	'touch.c',
)

add_project_arguments(
	'-DLIBINPUT_HAS_HOLD_GESTURES=@0@'.format(libinput.version().version_compare('>=1.19.0').to_int()),
	language: 'c',
)
