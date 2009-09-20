## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	-I../librawstudio/

AM_CFLAGS =\
	-Wall -fno-strict-aliasing\
	-O4\
	-DWITH_GCONF\
	-DDCRAW_NOMAIN\
	-DDCRAW_NOLCMS\
	-DWITH_MMAP_HACK

AM_CXXFLAGS = $(AM_CFLAGS)

uidir = $(datadir)/rawstudio/
ui_DATA = ui.xml rawstudio.gtkrc

bin_PROGRAMS = rawstudio

EXTRA_DIST = \
	$(ui_DATA)

rawstudio_SOURCES = \
	application.c application.h \
	rs-actions.c rs-actions.h \
	rs-preload.c rs-preload.h \
	gtk-helper.c gtk-helper.h \
	gtk-interface.c gtk-interface.h \
	rs-save-dialog.c rs-save-dialog.h \
	gtk-progress.c gtk-progress.h \
	conf_interface.c conf_interface.h \
	rs-cms.c rs-cms.h \
	rs-cache.c rs-cache.h \
	rs-batch.c rs-batch.h \
	rs-toolbox.c rs-toolbox.h \
	rs-navigator.c rs-navigator.h \
	rs-photo.c rs-photo.h \
	rs-jpeg.c rs-jpeg.h \
	rs-tiff.c rs-tiff.h \
	rs-arch.h \
	arch-generic.c \
	arch-x86.c \
	filename.c filename.h \
	rs-store.c rs-store.h \
	rs-preview-widget.c rs-preview-widget.h \
	rs-loupe.c rs-loupe.h \
	rs-histogram.c rs-histogram.h \
	eog-pixbuf-cell-renderer.c eog-pixbuf-cell-renderer.h \
	rs-pixbuf.c rs-pixbuf.h \
	rs-external-editor.c rs-external-editor.h \
	rs-dir-selector.c rs-dir-selector.h

rawstudio_LDADD = ../librawstudio/librawstudio-@VERSION@.la @PACKAGE_LIBS@ @LIBJPEG@ @LIBTIFF@ $(INTLLIBS)
