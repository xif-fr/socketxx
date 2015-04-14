SUBDIRS = handler io

AM_CXXFLAGS = @AM_CXXFLAGS@
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libsocketxx.la
libsocketxx_includedir = $(includedir)/socket++
libsocketxx_include_HEADERS = base_io.hpp base_unixsock.hpp base_inet.hpp quickdefs.h
libsocketxx_la_SOURCES = base_io.cpp base_unixsock.cpp base_inet.cpp
if SOCKETXX_ENABLE_SSL
libsocketxx_include_HEADERS += base_ssl.hpp 
libsocketxx_la_SOURCES += base_ssl.cpp 
endif
libsocketxx_libincludedir = $(libdir)/socket++/include
nodist_libsocketxx_libinclude_HEADERS = config.h
libsocketxx_la_LIBADD = io/libsocketxxio.la handler/libsocketxxhandlers.la