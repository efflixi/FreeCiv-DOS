## Process this file with automake to produce Makefile.in

noinst_LIBRARIES = libguiclient.a
INCLUDES = -I. -I$(srcdir)/.. -I$(srcdir)/../include -I$(top_srcdir)/common -I../../intl @CLIENT_CFLAGS@
AM_CFLAGS = -Werror=return-type -Werror=implicit-function-declaration \
	-Werror=incompatible-pointer-types -Werror=missing-prototypes

TESTS = dos_vbe_backend_check.sh vbe_check.sh dpmi_check.sh \
	framebuffer_check.sh mapview_check.sh
check_SCRIPTS = $(TESTS)
EXTRA_DIST = dos_vbe_backend_check.sh dos_client_stubs.c vbe_check.sh \
	build-vbe-check.sh tests/vbe_test.c tests/vbe_check.c dpmi_check.sh \
	tests/dpmi_test.c tests/dpmi-stubs/dpmi.h \
	tests/dpmi-stubs/sys/movedata.h tests/dpmi-stubs/sys/segments.h \
	framebuffer_check.sh tests/framebuffer_test.c mapview_check.sh \
	tests/mapview_test.c tests/map-stubs/control.h tests/map-stubs/map.h \
	tests/map-stubs/mapview.h build-render-check.sh tests/render_check.c \
	tests/render_pattern.c tests/render_pattern.h tests/render_reference.c

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
	gui_main.c \
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
	gui_main.h \
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
