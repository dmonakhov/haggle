bin_PROGRAMS=clitool
clitool_SOURCES=clitool.cpp prng.cpp

CPPFLAGS += -I$(top_builddir)/src/libhaggle/include 
CPPFLAGS += -I$(top_builddir)/src/utils 
CPPFLAGS += -I$(top_builddir)/src/libcpphaggle/include
LDFLAGS += -lhaggle -L$(top_builddir)/src/libhaggle/
LDFLAGS += -lhaggleutils -L$(top_builddir)/src/utils/
LDFLAGS += -lcpphaggle -L$(top_builddir)/src/libcpphaggle
LDFLAGS +=-lxml2

EXTRA_DIST= prng.h

CFLAGS += -std=gnu99

all-local:

clean-local:
	rm -f *~
