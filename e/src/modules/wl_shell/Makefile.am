MAINTAINERCLEANFILES = Makefile.in
MODULE = wl_shell

# data files for the module
filesdir = $(libdir)/enlightenment/modules/$(MODULE)
files_DATA = \
e-module-$(MODULE).edj module.desktop

EXTRA_DIST = $(files_DATA)

# the module .so file
INCLUDES               = -I. \
                         -I$(top_srcdir) \
                         -I$(top_srcdir)/src/modules/$(MODULE) \
                         -I$(top_srcdir)/src/bin/e_wayland \
                         -I$(top_builddir)/src/bin/e_wayland \
                         -I$(top_srcdir)/src/modules \
                         @e_wl_cflags@ @WAYLAND_SHELL_CFLAGS@

pkgdir                 = $(libdir)/enlightenment/modules/$(MODULE)/$(MODULE_ARCH)
pkg_LTLIBRARIES        = module.la

module_la_SOURCES      = e_mod_main.c \
                         e_mod_main.h

module_la_LIBADD       = @e_wl_libs@ @dlopen_libs@ @WAYLAND_SHELL_LIBS@
module_la_LDFLAGS      = -module -avoid-version
module_la_DEPENDENCIES = $(top_builddir)/config.h

uninstall:
	rm -rf $(DESTDIR)$(libdir)/enlightenment/modules/$(MODULE)
