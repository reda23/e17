
MAINTAINERCLEANFILES = Makefile.in

AM_CPPFLAGS = \
-I. \
-I$(top_srcdir)/src/lib \
-I$(top_srcdir)/src/lib/include \
-I$(top_srcdir)/src/modules/engines \
@FREETYPE_CFLAGS@ \
@PIXMAN_CFLAGS@ \
@FRIBIDI_CFLAGS@ \
@EVAS_GENERAL_CFLAGS@

if BUILD_ENGINE_SOFTWARE_8

SOFTWARE_8_SOURCES = evas_engine.c


if !EVAS_STATIC_BUILD_SOFTWARE_8_X11

pkgdir = $(libdir)/evas/modules/engines/software_8/$(MODULE_ARCH)
pkg_LTLIBRARIES = module.la
module_la_SOURCES = $(SOFTWARE_8_SOURCES)
module_la_LIBADD = @EVAS_GENERAL_LIBS@ $(top_builddir)/src/lib/libevas.la
module_la_LDFLAGS = -no-undefined @lt_enable_auto_import@ -module -avoid-version
module_la_LIBTOOLFLAGS = --tag=disable-static

else

noinst_LTLIBRARIES = libevas_engine_software_8.la

libevas_engine_software_8_la_SOURCES = $(SOFTWARE_8_SOURCES)

endif
endif

EXTRA_DIST = \
evas_engine.c
