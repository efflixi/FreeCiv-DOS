## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libguiclient.a
INCLUDES = -I. -I$(srcdir)/.. -I$(srcdir)/../include -I$(top_srcdir)/common -I../../intl @CLIENT_CFLAGS@
AM_CFLAGS = -Werror=return-type -Werror=implicit-function-declaration \
	-Werror=incompatible-pointer-types -Werror=missing-prototypes

TESTS = dos_vbe_backend_check.sh vbe_check.sh dpmi_check.sh \
	framebuffer_check.sh mapview_check.sh resource_check.sh input_check.sh \
	tests/widgets_check.sh event_loop_check.sh frontend_check.sh
check_SCRIPTS = $(TESTS)
EXTRA_DIST = dos_vbe_backend_check.sh dos_client_stubs.c vbe_check.sh \
	build-vbe-check.sh tests/vbe_test.c tests/vbe_check.c dpmi_check.sh \
	tests/dpmi_test.c tests/dpmi-stubs/dpmi.h \
	tests/dpmi-stubs/sys/movedata.h tests/dpmi-stubs/sys/segments.h \
	framebuffer_check.sh tests/framebuffer_test.c mapview_check.sh \
	tests/mapview_test.c tests/map-stubs/control.h tests/map-stubs/map.h \
	tests/map-stubs/mapview.h build-render-check.sh tests/render_check.c \
	tests/render_pattern.c tests/render_pattern.h tests/render_reference.c \
	resource_check.sh tests/resource_test.c tests/resource_manifest_test.py \
	tools/stage_resources.py build-map-check.sh tests/map_check.c \
	tests/phase6_scene.c tests/phase6_scene.h tests/map-config.h \
	tests/stage_phase6_fixture.py
EXTRA_DIST += input_check.sh tests/input_test.c tests/input-stubs/dpmi.h \
	tests/input-stubs/go32.h tests/input-stubs/unistd.h tests/input-stubs/sys/farptr.h \
	tests/widgets_check.sh tests/widgets_test.c event_loop_check.sh tests/event_loop_test.c \
	build-input-check.sh tests/input_check.c frontend_check.sh tests/frontend_test.c

## Above, note -I../../intl instead of -I$(top_srdir)/intl is deliberate.

libguiclient_a_SOURCES = \
	chatline.c \
	citydlg.c \
	cityrep.c \
	colors.c \
	connectdlg.c \
	dialogs.c \
	diplodlg.c \
	extender_compat.c \
	finddlg.c \
	framebuffer.c \
	bitmap_font.c \
	gotodlg.c \
	graphics.c \
	resource_xpm.c \
	gui_main.c \
	commands.c \
	event_loop.c \
	input.c \
	widgets.c \
	helpdlg.c \
	inteldlg.c \
	mapctrl.c \
	mapview.c \
	menu.c \
	messagedlg.c \
	messagewin.c \
	plrdlg.c \
	ratesdlg.c \
	repodlgs.c \
	spaceshipdlg.c \
	vbe_init.c \
	vbe_hw.c \
	wldlg.c \
	chatline.h \
	citydlg.h \
	cityrep.h \
	colors.h \
	connectdlg.h \
	dialogs.h \
	diplodlg.h \
	extender_compat.h \
	framebuffer.h \
	bitmap_font.h \
	gotodlg.h \
	graphics.h \
	resource_xpm.h \
	resource_colors.h \
	gui_main.h \
	commands.h \
	event_loop.h \
	input.h \
	widgets.h \
	helpdlg.h \
	mapctrl.h \
	mapview.h \
	menu.h \
	messagewin.h \
	plrdlg.h \
	repodlgs.h \
	spaceshipdlg.h \
	vbe_init.h \
	vbe_hw.h \
	wldlg.h
