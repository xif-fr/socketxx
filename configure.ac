AC_INIT([socket++], [3.10], [xcodexif@xif.fr], [socket++], [http://dev.xif.fr/socketxx])
AM_INIT_AUTOMAKE([foreign -Wall])
AC_CONFIG_HEADERS([socket++/config.h])
AC_CONFIG_SRCDIR([socket++/base_io.cpp])
AC_CONFIG_FILES([Makefile socket++/Makefile socket++/handler/Makefile socket++/io/Makefile socketxx.pc])
AC_CONFIG_MACRO_DIR([m4])

# Checks for compilers and programs
AC_PROG_CXX
AC_PROG_CC
AM_PROG_AR
LT_INIT

# Checks for compiler characteristics
AC_C_INLINE
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])
AC_C_BIGENDIAN

# Additional compiler and linker flags
AC_SUBST([AM_CXXFLAGS], "-I\$(top_srcdir)")

PKGCONFIG_ADD_LDFLAG=""
PKGCONFIG_ADD_DEP=""

AC_DEFINE([CONFIG_H_INCLUDED], [1], [config.h included])

# C++11 Standard Library
AC_ARG_ENABLE([cxx11-stdlib],
	[AS_HELP_STRING([--disable-cxx11-stdlib], [Don't use C++11 Standard Library features, like <fonctional> features])],
	[],
	[enable_cxx11_stdlib=yes]
)
AS_IF([test "x$enable_cxx11_stdlib" != xyes],
	[AC_DEFINE([XIF_NO_CXX11_STL], [1], [Disable C++11 Standard Library features])]
)

# Threads
AC_ARG_ENABLE([threads],
	[AS_HELP_STRING([--disable-threads], [Don't use threads features, like creating threads for socket server's client])],
	[],
	[enable_threads=yes]
)
if test "x$enable_threads" != xyes ; then
	AC_DEFINE([XIF_NO_THREADS], [1], [Disable threads features])
else
	PKGCONFIG_ADD_LDFLAG="$PKGCONFIG_ADD_LDFLAG -lpthread"
	AC_CHECK_HEADERS([pthread.h])
fi

# Stream operators
AC_ARG_ENABLE([stream-operators],
	[AS_HELP_STRING([--disable-stream-operators], [Disable stream-like operators])],
	[],
	[enable_stream_operators=yes]
)
AS_IF([test "x$enable_threads" != xyes],
	[AC_DEFINE([XIF_SOCKETXX_NO_STREAM_OPERATORS], [1], [Disable stream operators])]
)

# SSL/TLS Libraries
AC_ARG_WITH([openssl],
	[AS_HELP_STRING([--with-openssl], [add support for SSL/TLS encryption using OpenSSL library])],
	[],
	[with_openssl=yes]
)
if test "x$with_openssl" != xno ; then
	AC_CHECK_LIB([ssl], [SSL_new],
		[AC_DEFINE([XIF_USE_SSL], [1], [Define if OpenSSL library is used])],
		[AC_MSG_FAILURE([" *** OpenSSL library not found (--without-openssl to disable)."])]
	)
	PKGCONFIG_ADD_LDFLAG="$PKGCONFIG_ADD_LDFLAG -lssl -lcrypto"
	PKGCONFIG_ADD_DEP="$PKGCONFIG_ADD_DEP libssl"
	AC_CHECK_LIB([ssl], [TLS_client_method], [], [
		AC_DEFINE([TLS_client_method], [TLSv1_2_client_method], [TLS_client_method fallback to TLSv1.2])
		AC_DEFINE([TLS_server_method], [TLSv1_2_server_method], [TLS_server_method fallback to TLSv1.2])
		AC_MSG_WARN([TLS flexible method not avalaible, falling back to TLSv1.2])
	])
fi
AM_CONDITIONAL([SOCKETXX_ENABLE_SSL], [test "x$with_openssl" == xyes])

# Pkgconfig file
AC_SUBST([PKGCONFIG_ADD_LDFLAG])
AC_SUBST([PKGCONFIG_ADD_DEP])

# Checks for typedefs, structures
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for header files
AC_CHECK_HEADERS([inttypes.h unistd.h fcntl.h errno.h netinet/tcp.h netdb.h arpa/inet.h netinet/in.h sys/un.h sys/stat.h sys/socket.h sys/types.h sys/mman.h])

# Chech for MSG_NOSIGNAL
AC_MSG_CHECKING([for MSG_NOSIGNAL flag presence])
AC_EGREP_CPP(yes_have_msg_signal_flag, [
#include <sys/socket.h>
#ifdef MSG_NOSIGNAL
yes_have_msg_signal_flag
#endif
], [
	AC_MSG_RESULT([yes])
], [
	AC_MSG_RESULT([no])
	AC_MSG_WARN([[MSG_NOSIGNAL flag does not exist on this system. Be aware of SIGPIPEs !]])
	AC_DEFINE([MSG_NOSIGNAL], [0], [No support of MSG_NOSIGNAL flag ?])
])

# Checks for library functions
AC_CHECK_FUNCS([strerror recv send setsockopt getsockopt shutdown read write close fstat fcntl socket munmap mmap lseek getpagesize open l64a clock rand dup accept listen bind select connect gethostbyname inet_pton unlink socketpair strlen])

AC_OUTPUT
