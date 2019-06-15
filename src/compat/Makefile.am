noinst_LTLIBRARIES = libcompat.la

libcompat_la_SOURCES = \
	mman.c \
	mman.h \
	uuid.c \
	uuid.h

libcompat_la_LDFLAGS = \
	$(LD_NO_AS_NEEDED)

if BABELTRACE_BUILD_WITH_MINGW
libcompat_la_LDFLAGS += -lrpcrt4
endif

noinst_HEADERS = \
	bitfield.h \
	compiler.h \
	endian.h \
	fcntl.h \
	glib.h \
	limits.h \
	memstream.h \
	socket.h \
	stdio.h \
	stdlib.h \
	string.h \
	time.h \
	unistd.h \
	utc.h
