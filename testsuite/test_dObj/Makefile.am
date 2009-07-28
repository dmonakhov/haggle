.PHONY: \
	test \
	testgetputData

HAGGLE_KERNEL_DIR=$(top_srcdir)/src/hagglekernel/
UTILS_DIR=$(top_srcdir)/src/utils/
LIBCPPHAGGLE_DIR=$(top_srcdir)/src/libcpphaggle/
CPPFLAGS += -I$(HAGGLE_KERNEL_DIR) -I$(UTILS_DIR) -I.. -I$(LIBCPPHAGGLE_DIR)include/ -I$(LIBXML2_INCLUDE_DIR)
LDFLAGS = -lxml2
if OS_LINUX
LDFLAGS += -lpthread
endif

bin_PROGRAMS= \
	getputData

STDDEPS=$(HAGGLE_KERNEL_DIR)libhagglekernel.a
STDDEPS+=$(UTILS_DIR)libhaggleutils.a
STDDEPS+=../libtesthlp.a

getputData_SOURCES=getputData.cpp
getputData_DEPENDENCIES=$(STDDEPS)

LDADD=$(HAGGLE_KERNEL_DIR)libhagglekernel.a 
LDADD+=$(UTILS_DIR)libhaggleutils.a
LDADD+=$(LIBCPPHAGGLE_DIR)libcpphaggle.a
LDADD+=../libtesthlp.a

if OS_MACOSX
LDFLAGS += -framework IOKit -framework CoreFoundation -framework CoreServices
endif

test: \
	testgetputData

testgetputData: getputData
	@./getputData && echo "Passed!" || echo "Failed!"

all-local:

clean-local:
	rm -f *~ *.o
