SUBDIRS = \
	utils \
	libcpphaggle \
	hagglekernel \
	libhaggle \
	experimental  \
	bbs \
	webserver \
	proxy \
	clitool \
	luckyMe \
	tests/test01

if OS_LINUX
if HAVE_INOTIFY
SUBDIRS += filedrop
endif
else
# not Linux
SUBDIRS += filedrop
endif

clean-local:
	rm -f *~
