bin_PROGRAMS=webserver
webserver_SOURCES=webserver.cpp

CPPFLAGS += -I$(top_builddir)/src/libhaggle/include
LDFLAGS += -lhaggle -lpthread -L$(top_builddir)/src/libhaggle/
LDFLAGS +=-lxml2

all-local:

clean-local:
	rm -f *~
