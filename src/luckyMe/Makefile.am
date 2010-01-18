bin_PROGRAMS=luckyMe
luckyMe_SOURCES=luckyme.c

CPPFLAGS +=-I$(top_builddir)/src/libhaggle/include 
CPPFLAGS +=-I$(top_builddir)/src/utils 
LDFLAGS +=-lhaggle -L$(top_builddir)/src/libhaggle/
LDFLAGS +=-lhaggleutils -L$(top_builddir)/src/utils/
LDFLAGS +=-lxml2

CFLAGS += -std=gnu99

all-local:

clean-local:
	rm -f *~
