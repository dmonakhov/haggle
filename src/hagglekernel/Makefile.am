# This target is the final Haggle executable
bin_PROGRAMS:=haggle
haggle_SOURCES:=main.cpp
haggle_DEPENDENCIES:=$(top_srcdir)/config.h libhagglekernel.a $(top_builddir)/src/utils/libhaggleutils.a $(top_builddir)/src/libcpphaggle/libcpphaggle.a
haggle_LDADD:=libhagglekernel.a $(top_builddir)/src/utils/libhaggleutils.a $(top_builddir)/src/libcpphaggle/libcpphaggle.a

# This target is an intermediate library, that can be reused by the testsuite
noinst_LIBRARIES:=libhagglekernel.a
libhagglekernel_a_SOURCES = \
	Filter.cpp \
	Event.cpp \
	Attribute.cpp \
	Bloomfilter.cpp \
	DataObject.cpp \
	Node.cpp \
	Address.cpp \
	Interface.cpp \
	Certificate.cpp \
	RepositoryEntry.cpp \
	NodeStore.cpp \
	InterfaceStore.cpp \
	DataStore.cpp \
	SQLDataStore.cpp \
	Metadata.cpp \
	XMLMetadata.cpp \
	MetadataParser.cpp \
	HaggleKernel.cpp \
	ConnectivityInterfacePolicy.cpp \
	Queue.cpp \
	Policy.cpp \
	Manager.cpp \
	ManagerModule.cpp \
	NodeManager.cpp \
	DataManager.cpp \
	ProtocolManager.cpp \
	ForwardingManager.cpp \
	SecurityManager.cpp \
	ForwarderAsynchronous.cpp \
	ForwarderProphet.cpp \
	Connectivity.cpp \
	ConnectivityLocal.cpp \
	ConnectivityManager.cpp \
	ApplicationManager.cpp \
	Protocol.cpp \
	ProtocolSocket.cpp \
	ProtocolUDP.cpp \
	ProtocolTCP.cpp \
	ProtocolLOCAL.cpp \
	ResourceManager.cpp \
	Trace.cpp \
	Utility.cpp

libhagglekernel_a_DEPENDENCIES=$(top_srcdir)/config.h

EXTRA_DIST_OTHER_PLATFORMS = \
	ConnectivityLocalAndroid.h \
	ConnectivityLocalAndroid.cpp \
	ConnectivityLocalWindowsXP.h \
	ConnectivityLocalWindowsXP.cpp \
	ConnectivityLocalWindowsMobile.h \
	ConnectivityLocalWindowsMobile.cpp \
	ConnectivityBluetoothWindowsXP.h \
	ConnectivityBluetoothWindowsXP.cpp \
	ConnectivityBluetoothWindowsMobile.h \
	ConnectivityBluetoothWindowsMobile.cpp \
	ConnectivityBluetoothWindowsMobileWIDCOMM.h \
	ConnectivityBluetoothWindowsMobileWIDCOMM.cpp \
	ProtocolRFCOMMWIDCOMM.h \
	ProtocolRFCOMMWIDCOMM.cpp \
	ResourceMonitorAndroid.h \
	ResourceMonitorAndroid.cpp \
	ResourceMonitorWindowsMobile.h \
	ResourceMonitorWindowsMobile.cpp \
	ResourceMonitorWindowsXP.h \
	ResourceMonitorWindowsXP.cpp \
	Android.mk

EXTRA_DIST= Attribute.h \
	Bloomfilter.h \
	ApplicationManager.h \
	ConnectivityManager.h \
	Connectivity.h \
	ConnectivityLocal.h \
	ConnectivityLocalMacOSX.h \
	ConnectivityLocalLinux.h \
	ConnectivityBluetooth.h \
	ConnectivityBluetoothLinux.h \
	ConnectivityBluetoothMacOSX.h \
	ConnectivityEthernet.h \
	ConnectivityMedia.h \
	Forwarder.h \
	ForwarderEmpty.h \
	ForwarderProphet.h \
	ForwarderAsynchronous.h \
	DataManager.h \
	DataObject.h \
	DataStore.h \
	SQLDataStore.h \
	Certificate.h \
	NodeStore.h \
	InterfaceStore.h \
	DebugManager.h \
	Debug.h \
	BenchmarkManager.h \
	Event.h \
	EventQueue.h \
	Filter.h \
	HaggleKernel.h \
	ConnectivityInterfacePolicy.h \
	Queue.h \
	Address.h \
	Interface.h \
	Manager.h \
	ManagerModule.h \
	Metadata.h \
	MetadataParser.h \
	Node.h \
	NodeManager.h \
	SecurityManager.h \
	Protocol.h \
	ProtocolSocket.h \
	ProtocolLOCAL.h \
	ProtocolManager.h \
	ForwardingManager.h \
	ProtocolRAW.h \
	ProtocolTCP.h \
	ProtocolUDP.h \
	ProtocolRFCOMM.h \
	ProtocolRFCOMMMacOSX.h \
	ProtocolRFCOMMMacOSX.mm \
	ProtocolMedia.h \
	Policy.h \
	RepositoryEntry.h \
	XMLMetadata.h \
	ResourceManager.h \
	ResourceMonitor.h \
	ResourceMonitorLinux.h \
	ResourceMonitorMacOSX.h \
	Trace.h \
	Utility.h \
	$(EXTRA_DIST_OTHER_PLATFORMS) \
	Doxyfile.in \
	hagglesim


haggle_LDFLAGS :=-L../utils/ -L../libcpphaggle -lhaggleutils -lcpphaggle -lm $(XML_LIBS) -lcrypto
haggle_CXXFLAGS :=$(CXXFLAGS)
haggle_CPPFLAGS :=$(XML_CPPFLAGS) -I$(top_srcdir)/src/utils -I$(top_srcdir)/src/libcpphaggle/include -I$(top_srcdir)/src/libhaggle/include
libhagglekernel_a_CPPFLAGS:=$(haggle_CPPFLAGS)

if HAVE_EXCEPTION
haggle_CXXFLAGS +=-DHAVE_EXCEPTION=1
else
haggle_CXXFLAGS +=-DHAVE_EXCEPTION=0 -fno-exceptions -fno-non-call-exceptions
endif

# Configuration dependent source code
if DEBUG_LEAKS
libhagglekernel_a_SOURCES += Debug.cpp DebugManager.cpp 
endif

if DEBUG
libhagglekernel_a_SOURCES += Debug.cpp DebugManager.cpp 
endif

if BENCHMARK
libhagglekernel_a_SOURCES += BenchmarkManager.cpp
endif

# Linux specific source code
if OS_LINUX

haggle_LDFLAGS += -lpthread

libhagglekernel_a_SOURCES += ConnectivityLocalLinux.cpp ResourceMonitorLinux.cpp

if ENABLE_BLUETOOTH
libhagglekernel_a_SOURCES += ConnectivityBluetooth.cpp ConnectivityBluetoothLinux.cpp ProtocolRFCOMM.cpp
endif

if ENABLE_ETHERNET
libhagglekernel_a_SOURCES += ConnectivityEthernet.cpp
endif

if HAVE_DBUS
haggle_LDFLAGS += $(DBUS_LIBS)
haggle_CPPFLAGS += $(DBUS_INCLUDES)
endif


endif # OS_LINUX

# Objective C++ sources for Mac OS X APIs
OBJCXX_SRC = ProtocolRFCOMMMacOSX.mm
OBJCXX_SRC_CPP := $(OBJCXX_SRC:%.mm=%.cpp)

# Link the objective C++ source (.mm) to a .cpp file. Automake does
# not seem to recognize Objective C++ files.
$(OBJCXX_SRC_CPP): %.cpp: %.mm
	ln -s $< $@

# Mac OS X specific source code
if OS_MACOSX

libhagglekernel_a_SOURCES += ConnectivityLocalMacOSX.cpp ResourceMonitorMacOSX.cpp

if OS_IPHONEOS
haggle_LDFLAGS +=-framework CoreFoundation -L@OPENSSL_LIB_DIR@
else
haggle_LDFLAGS +=-framework CoreFoundation -framework IOKit -framework CoreServices
endif

if ENABLE_BLUETOOTH

libhagglekernel_a_SOURCES += ConnectivityBluetooth.cpp ConnectivityBluetoothMacOSX.cpp

noinst_LIBRARIES += libmacosxobjc.a
haggle_DEPENDENCIES += libmacosxobjc.a

libmacosxobjc_a_SOURCES := ProtocolRFCOMMMacOSX.cpp
libmacosxobjc_a_DEPENDENCIES=$(top_srcdir)/config.h

libmacosxobjc_a_CPPFLAGS :=$(haggle_CPPFLAGS) -x objective-c++ 
#libmacosxobjc_a_CXXFLAGS = 
haggle_LDADD += libmacosxobjc.a
haggle_LDFLAGS +=-framework IOBluetooth -framework Foundation
endif

if ENABLE_ETHERNET
libhagglekernel_a_SOURCES += ConnectivityEthernet.cpp
endif

if ENABLE_MEDIA
libhagglekernel_a_SOURCES += ConnectivityMediaMacOSX.cpp ProtocolMedia.cpp
endif

endif # OS_MACOSX

if ENABLE_ETHERNET
libhagglekernel_a_SOURCES += ConnectivityEthernet.cpp
endif

if OMNETPP
# OMNet++ support does not work in its current state
libhaggleopp_a_CPPFLAGS = -DOMNETPP 
noinst_LIBRARIES += libhaggleopp.a
libhaggleopp_a_SOURCES = HaggleKernel.cpp \
			Event.cpp \
			Node.cpp \
			DataObject.cpp \
			Interface.cpp \
			Attribute.cpp \
			DataManager.cpp \
			NodeManager.cpp \
			ProtocolManager.cpp \
			ConnectivityManager.cpp
endif

if BUNDLED_SQLITE
CPPFLAGS +=-I@SQLITE_INCLUDE_DIR@
haggle_LDADD += @SQLITE_LIB_DIR@/libsqlite.a
else
haggle_LDFLAGS +=-lsqlite3 
endif

if ENABLE_BLUETOOTH
if OS_LINUX
haggle_LDFLAGS +=-lbluetooth
endif

if OS_MACOSX
haggle_LDFLAGS +=-framework IOBluetooth
endif
endif

if OS_MACOSX
haggle_LDFLAGS +=-framework CoreFoundation
endif

all-local:
	cp $(PROGRAMS) $(top_builddir)/bin/

indent:
	indent -kr -i8 $(wildcard *.cpp) $(wildcard *.h)

clean-local:
	rm -f *~
	rm -f hagglesim/*~
	rm -f hagglesim/*_n.cc
	rm -f hagglesim/*_n.o
if OS_MACOSX
	for f in $(libmacosxobjc_a_SOURCES); do \
		rm -f $(f); \
	done
endif

# Clean up the hagglesim dir
dist-hook:
	rm -rf `find $(distdir)/hagglesim -name .svn`
	rm -f $(distdir)/hagglesim/*~
	rm -f $(distdir)/hagglesim/*_n.cc
	rm -f $(distdir)/hagglesim/*_n.o
