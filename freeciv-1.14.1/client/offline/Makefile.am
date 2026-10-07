noinst_LIBRARIES = liboffline.a
INCLUDES = -I$(top_srcdir)/common -I$(top_builddir)/intl
liboffline_a_SOURCES = local_queue.c local_queue.h session.c session.h engine.h
EXTRA_DIST = engine.c build-engine.sh check.sh transport_test.c \
	runtime_check.c build-runtime-check.sh

ENGINE_INPUTS = $(wildcard $(top_srcdir)/common/*.[ch]) \
	$(wildcard $(top_srcdir)/server/*.[ch]) \
	$(wildcard $(top_srcdir)/ai/*.[ch]) \
	$(srcdir)/engine.c $(srcdir)/engine.h $(srcdir)/build-engine.sh \
	$(top_builddir)/config.h

all-local: engine-build/engine.o

engine-build/engine.o: $(ENGINE_INPUTS)
	CC="$(CC)" LD="$(LD)" NM="$(NM)" OBJCOPY="$(OBJCOPY)" \
	CPPFLAGS="$(CPPFLAGS)" CFLAGS="$(CFLAGS)" \
	$(SHELL) "$(srcdir)/build-engine.sh" "$(abs_builddir)/engine-build" \
	"$(abs_top_builddir)"

clean-local:
	rm -f engine-build/engine.o engine-build/engine.raw.o \
		engine-build/engine.o.tmp engine-build/engine.defined \
		engine-build/engine.symbols engine-build/engine.isolated \
		engine-build/common/*.o engine-build/engine/*.o
