if OS_IPHONEOS
else
if OS_LINUX
bin_PROGRAMS=filedrop
filedrop_SOURCES=filedrop.c
endif

if OS_MACOSX
bin_PROGRAMS=filedrop
filedrop_SOURCES=filedropMacOSX.c
LDFLAGS += -framework CoreServices
endif

CPPFLAGS += -I$(top_builddir)/src/libhaggle/include
LDFLAGS += -lhaggle -L$(top_builddir)/src/libhaggle/
LDFLAGS +=-lxml2

EXTRA_DIST = filedropMacOSX.c filedrop.c

all-local:
	cp $(PROGRAMS) $(top_builddir)/bin/

clean-local:
	rm -f *~

endif 
