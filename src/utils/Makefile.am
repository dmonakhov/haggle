noinst_LIBRARIES = libhaggleutils.a
libhaggleutils_a_SOURCES = utils.c bloomfilter.c counting_bloomfilter.c base64.c prng.c
EXTRA_DIST = utils.h bloomfilter.h counting_bloomfilter.h base64.h haggleutils.h prng.h

CFLAGS += -std=gnu99
LDADD = -lcrypto

#all-local:
