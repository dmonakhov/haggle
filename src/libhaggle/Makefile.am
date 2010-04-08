lib_LTLIBRARIES = libhaggle.la
libhaggle_la_SOURCES = dataobject.c ipc.c attribute.c debug.c node.c base64.c interface.c platform.c sha1.c metadata.c metadata_xml.c

if JAVA_BINDINGS
# Descend into subdirectories if we have Java bindings enabled
SUBDIRS = . jni # build the current directory first
endif

EXTRA_DIST = include base64.h sha1.h sha1_private.h metadata.h metadata_xml.h Android.mk Doxyfile.in

libhaggle_la_LDFLAGS =-lpthread -lxml2 
libhaggle_la_CFLAGS =-std=gnu99
libhaggle_la_CPPFLAGS =$(XML_CPPFLAGS) -Iinclude

if HAVE_EXTRACTOR
libhaggle_la_LDFLAGS += -lextractor
endif

clean-local:
	rm -f *~
