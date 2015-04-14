/**********************************************************************
 * Xif Socket++ Networking Library
 **********************************************************************
 * Copyright Félix Faisant 2012 - 2014
 * Software under GNU LGPL http://www.gnu.org/licenses/lgpl.html
 * For bug reports, help, or improve requests, please mail at Félix Faisant <xcodexif@xif.fr>
 **********************************************************************
 * Socket++ is a C++ warper library for Input/Output systems, especially TCP/IP sockets
 * 
 * This library is very modular and is shipped with some classes. Here is a short description.
 *  Full description of these classes can be found in respective headers.
 *
 * There are three groups of classes : BaseIO classes, IO protocols, and Connection Handlers.
 *
 * BaseIO classes are RAII objects for holding and manipulating underlying ressources, like 
 *  file descriptors, files, sockets, Windows HANDLEs... They are responsible for the ressources
 *  and must implement some basic I/O methods for reading and writing. They are also responsible
 *  for ressource flags/options/special operations. There may be a BaseIO for any thing in which
 *  we can write/send and (or?) read/receive data.
 * Shipped BaseIO classes :
 *    - BaseFD : root handler for UNIX file descriptors / Windows HANDLEs
 *    - BaseSocket : handler for sockets. Subclasses  propose an ::addr_info struct for a certain AF.
 *      - BaseNetSock : TCP/IP sockets (AF_INET)
 *        - BaseSSL : handler for SSL sockets : SSL mode can be switched on/off at any time
 *      - BaseUnixSock : UNIX (local) sockets (AF_UNIX)
 *    - BasePipe : handler for UNIX pipes / Windows Named pipes / Windows anonymous pipes
 *    - BaseFile : handler for files and virtual files like stdin/stdout
 *
 * IO protocols (or types) classes are objects used by user for reading/writing. They must NOT 
 *  implement a specific protocol (OSI's Application Layer, like HTTP) but generic familly of protocols.
 *  They are the equivalent of OSI's Presentation Layer.
 * These generic protocols are shipped with socket++ :
 *    - Simple Socket : It is designed to be fast, stupid, and simple, free of transport problems.
 *                      It can transport different basic things : strings of any size, 
 *                       integers of all sizes without caring of endianness, a simple byte,
 *                       files, a socket itself, a xif::polyvar, and raw data.
 *                      The only thing you need to care about is the type of data and order.
 *                      It is perfect for personal & simple protocols, with good performances.
 *    - Stoppable Simple Socket : Overloaded Simple Socket for event paradigm. The read waiting 
 *                                operation can be stopped by an another file descriptor, 
 *                                for example a pipe between threads, or stdio.
 *    - Text Socket : For use of plain old textual protocols like SMTP or HTTP.
 *                      You can define the line ending freely. It is line-oriented.
 *    - Tunnel : simply copy data between two streams, using, if possible, zero-copy facilities
 * 
 * These IO protocols are used with Connection Handlers (or 'ends'). A Connection Handler is only responsible 
 *  for session-related operations and opening/connecting/managing. It inherits from an IO protocol because
 *  it can need to know the complete socket++ object; but has nothing to do with the protocol or datas.
 * These Connection Handlers are shipped with socket++ :
 *    - The socket "server" side, or incoming side :
 *        Listen on an address and wait for incoming connections.
 *        Manage clients. These client object can hold an attached data. You can store 
 *         custom informations in it. It uses, like any socket++ classes (see below), reference counting. 
 *         The server object can retain for you connected clients.
 *        Clients can be put in pools waiting for client activity, in autonomous threads, 
 *         be processed by a callback funtion, or one by one.
 *    - The socket "client" side, or outcoming side :
 *        Simply connects to a specified address.
 * 
 * A complete (usable) socket++ object is declared like this : ConnectionHandler<IOProtocol<BaseIO>>
 * Example : socketxx::handler_socket::client<socketxx::io::simple_socket<socketxx::base_ssl>> cli(...);
 * <socketxx/quickdefs.h> can be included after other headers for shortened types.
 * Most of the time, once a socket is connected, an object of type IOProtocol<BaseIO> can be used.
 * Also, socket objects can be constructed with other socket objects, and BaseIOs objects can be up/down/casted
 *  if you know what you're doing (eg. base_socket -> base_netsock if the underlying socket is AF_INET)
 *
 * The whole library error/event reporting is based on exceptions.
 * All the objetcs in this library are reference-counted, transparently. You have just to remember that 
 *  you are free to copy socket objects. There are no pointers or object handlers.
 *
 * -== To Do : IPv6 full support - P2P Mode - Socket states ==-
 *
 **********************************************************************/

#ifndef SOCKET_XX_BASE_H
#define SOCKET_XX_BASE_H

	/// Config header
#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

	/// General headers
#include <stdlib.h>
#include <inttypes.h>
#include <errno.h>
#include <string>
#include <stdexcept>
#include <xifutils/traits.hpp>

	/// OS specific headers
#ifdef _WIN32 // Windows
	#include <windows.h>
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#include <io.h>
	#pragma comment(lib, "ws2_32.lib")
	typedef SOCKET socket_t;
	typedef HANDLE fd_t;
	typedef int socklen_t;
	typedef SOCKADDR_IN sockaddr_in;
	typedef SOCKADDR sockaddr;
	typedef unsigned int sa_family_t
	#define _SOCKETXX_IS_INVALID_HANDLE(fd) if (fd == INVALID_HANDLE) 
	#define _SOCKETXX_IS_INVALID_SOCKET(fd) if (fd == INVALID_SOCKET) 
	#define socket_errno WSAGetLastError()
	#define socket_errno_reset WSASetLastError(0)
	#define errno_reset errno = 0
	struct _socketxx_winsock_init {
		WSADATA WSAData;
		_socketxx_winsock_init() { WSAStartup(MAKEWORD(2,2), &WSAData); }
		~_socketxx_winsock_init() { WSACleanup(); }
	};
	#define _SOCKETXX_WIN_DELETE = delete
	#define _SOCKETXX_UNIX_DELETE 
	#define _SOCKETXX_WIN_IMPL(f) f
	#define _SOCKETXX_UNIX_IMPL(f) = delete
	#error Winsocks not supported for the moment
#else // UNIXs
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/ioctl.h>
	#include <sys/stat.h>
	#include <fcntl.h>
	typedef int fd_t;
	typedef fd_t socket_t;
	#define INVALID_SOCKET -1
	#define INVALID_HANDLE -1
	#define _SOCKETXX_IS_INVALID_HANDLE(fd) if (fd < 0) 
	#define _SOCKETXX_IS_INVALID_SOCKET(fd) if (fd < 0) 
	#define SOCKET_ERROR -1
	#define socket_errno errno
	#define errno_reset errno = 0  // Which expands to `*__error()` = 0, so `errno = 0` is valid
	#define socket_errno_reset errno_reset
	#define _SOCKETXX_WIN_DELETE 
	#define _SOCKETXX_UNIX_DELETE = delete
	#define _SOCKETXX_WIN_IMPL(f) = delete
	#define _SOCKETXX_UNIX_IMPL(f) f
#endif

	/// Endianness
// Define endianness (from Autoconf's WORDS_BIGENDIAN or system defs)
#define XIF_SOCKETXX_BIG_ENDIAN 1
#define XIF_SOCKETXX_LITTLE_ENDIAN 0
#ifdef CONFIG_H_INCLUDED
	#ifdef WORDS_BIGENDIAN
		#define XIF_SOCKETXX_ENDIANNESS XIF_SOCKETXX_BIG_ENDIAN
	#else
		#define XIF_SOCKETXX_ENDIANNESS XIF_SOCKETXX_LITTLE_ENDIAN
	#endif
#else
	#ifndef XIF_SOCKETXX_ENDIANNESS
		#warning TO DO : Endianness without Autoconf's WORDS_BIGENDIAN
	#endif
#endif
// Endianness priority. /!\WARNING/!\ If socket++ is build with big-endian priority, integer routines
//  will not be compatible with little-endian socket++ libs. Change this only for isolated network system.
#define XIF_SOCKETXX_ENDIANNESS_PRIORITY XIF_SOCKETXX_LITTLE_ENDIAN  // Little endian priority (Intel rules the world)
#define XIF_SOCKETXX_ENDIANNESS_SAME XIF_SOCKETXX_ENDIANNESS_PRIORITY != XIF_SOCKETXX_ENDIANNESS

	/// Threads (pthread)
#ifndef XIF_NO_THREADS
	#include <pthread.h>
#endif

	/// Reference counting
#include <xifutils/refcount++.hpp>

	/// Debugging
#if DEBUG
	#include <xifdebug/xifdebug.hpp>
	#include <xifdebug/on.h>
#else
	#define dfunc(level, func_name)
	#define dinfo(level, str)
	#define dformat(level, format, ...)
	#define dvar(level, var)
#endif

	/// Other
// Timeval comparing
inline bool operator== (timeval first, timeval second) { return (first.tv_sec == second.tv_sec) && (first.tv_usec == second.tv_usec); }
inline bool operator!= (timeval first, timeval second) { return !(first == second); }
// Timeval 0
#define NULL_TIMEVAL timeval({0,0})

namespace socketxx {
	
		// Addr use types
	enum class _addr_use_type_t {
		SERVER,
		SERVER_CLI,
		CLIENT,
	};
	
		// Autodelete binary data buffer, can be usefull
	struct auto_bdata : public refcountxx_base {
		void* p;
		size_t len;
		auto_bdata () : p(NULL), len(0) {}
		auto_bdata (void* p) : p(p) {}
		auto_bdata (const auto_bdata& o) : refcountxx_base(o), p(o.p), len(o.len) {}
		~auto_bdata() { if (this->can_destruct() and p != NULL) { delete[] (unsigned char*)p; } }
	};
	
		// Simple flags helper for adding or clearing flags
	class flags {
	protected:
		virtual int get () const = 0;
		virtual void set (int) = 0;
		virtual ~flags() {};
		#define _SOCKETXX_FLAGS_COMMON(class) protected: virtual int get () const; virtual void set (int); public: virtual ~class();
	public:
		flags& operator+= (int flag) { (*this) |= flag; return *this; }
		flags& operator-= (int flag) { (*this) &= ~flag; return *this; }
		flags& operator&= (int flags) { int f = this->get(); f &= flags; this->set(f); return *this; }
		flags& operator|= (int flags) { int f = this->get(); f |= flags; this->set(f); return *this; }
		flags& operator= (int flags) { this->set(flags); return *this; }
		bool operator& (int flag) const { int f = this->get(); return f & flag; }
	};
	
		///// Event Exceptions /////
	
		// Generic socket++ event exception
	class event : public std::exception {
	public:
		event () noexcept : std::exception() {}
		virtual const char* what() const noexcept = 0;
		virtual ~event () noexcept {};
	};
	
		// Stop exception (synchronous I/O or waiting opperation interrupted by a monitored file descriptor)
	class stop_event : socketxx::event {
		fd_t _awaked_fd;
	public:
		stop_event (fd_t awaked_fd) noexcept : event(), _awaked_fd(awaked_fd) {}
		virtual const char* what() const noexcept;
		fd_t get_awaked_fd () const noexcept { return _awaked_fd; }
	};
	
		// Timeout event exception
	class timeout_event : socketxx::event {
	public:
		timeout_event () noexcept : event() {}
		virtual const char* what() const noexcept;
	};
	
		///// Error Exceptions /////
	
		// Generic socket++ error exception
	class error : public std::exception {
	protected:
		std::string descr;
		error (std::string descr) noexcept : descr(descr) {}
		error () noexcept : descr("socket++ error") {}
	public:
		explicit error (const char* descr) noexcept : descr(descr) {}
		virtual const char* what () const noexcept { return descr.c_str(); }
		virtual ~error() noexcept {}
	};
	
		// Generic classic system or socket exception, using `errno`
	class classic_error : public socketxx::error {
	protected:
		int std_errno;
		static std::string _errno_str (int std_errno) noexcept;
		classic_error (std::string descr, int err) noexcept : error(descr+_errno_str(err)), std_errno(err) {}
		classic_error (int err) noexcept : error(), std_errno(err) {}
	public:
		int get_errno () const { return std_errno; }
		virtual ~classic_error() noexcept;
	};
	
		// In-Out socket exception
	class io_error : public socketxx::classic_error {
	private:
		std::string _str () const noexcept;
	public:
		enum _type { READ = 0, WRITE = 1 } err_type;
		int ret;
		io_error (ssize_t ret, _type t) noexcept : err_type(t), ret((int)ret), classic_error(socket_errno) { socket_errno_reset; this->descr = this->_str(); }  // For socket I/O
		io_error (_type t, bool incompl_read = false) noexcept : err_type(t), ret((incompl_read)?1:SOCKET_ERROR), classic_error(errno) { errno_reset; this->descr = this->_str(); }  // For non-socket I/O
		bool is_connection_closed () const noexcept { return ret == 0; }	// `false` don't ensure that the connection is still opened !
		bool is_timeout_error () const noexcept { return ret == ETIMEDOUT; }
	};
	
		// Invalid socket or file descriptor exception or failed socket creation
	class bad_socket_error : public socketxx::classic_error {
	private:
		std::string _str () const noexcept;
	public:
		bool fd_from_user;
		enum fd_type_t { SOCKET, PIPE, FD } fd_type;
		bad_socket_error(fd_type_t t, bool from_user = false) noexcept : fd_from_user(from_user), fd_type(t), classic_error(t==SOCKET?socket_errno:errno) { if (t==SOCKET) errno_reset; else socket_errno_reset; this->descr = this->_str(); }
	};
	
		// Other socket or syscall error
	class other_error : public socketxx::classic_error {
	public:
		other_error(std::string what, bool socket_op) noexcept : classic_error(what,socket_op?socket_errno:errno) { if (socket_op) errno_reset; else socket_errno_reset; }
	};
	
		///------ Base class for simple file descriptors (stdin, pipe, files...) ------///
	#define SOCKETXX_AUTO_CLOSE (bool)true
	#define SOCKETXX_MANUAL_FD (bool)false
	class base_fd {
	protected:
			// Using refcounting
		REFCXX_REFCOUNTED(base_fd);
		
			// The file descriptor
		fd_t fd;
			// Shared socket settings
		struct _shrd_data {
			// Autoclose file descriptor ?
			bool autoclose;
			// Preserve the inode from future automatic actions
			bool preserve_fd;
/*			// Aggregation
 uint8_t cork_lvl;
 // http://baus.net/on-tcp_cork/
 #warning TO DO : multithreading, TCP_CORK*/
			_shrd_data () : autoclose(true), preserve_fd(false) {}
			_shrd_data (bool autoclose) : autoclose(autoclose), preserve_fd(false) {}
		} * shd;
		
			// Private initialization
		base_fd () : REFCXX_CONSTRUCTOR(base_fd), fd(INVALID_HANDLE), shd(new _shrd_data()) {}
		base_fd (bool autoclose_handle, fd_t handle) : REFCXX_CONSTRUCTOR(base_fd), fd(handle), shd(new _shrd_data(autoclose_handle)) {}  // Don't forget to check file descriptor
		
	public:
			// Public constructors with already created file descriptor
		base_fd (fd_t handle, bool autoclose_handle) : REFCXX_CONSTRUCTOR(base_fd), fd(handle), shd(new _shrd_data(autoclose_handle)) { _SOCKETXX_IS_INVALID_HANDLE(fd) throw socketxx::bad_socket_error(bad_socket_error::FD, true); }
			// Copy constructor (with reference counting) /!\ base_fd::fd is not const, do NOT copy before the socket is connected or binded !
		base_fd (const base_fd& o) : REFCXX_COPY_CONSTRUCTOR(base_fd, o), fd(o.fd), shd(o.shd) {}
			// Assign reconstructor
		REFCXX_ASSIGN(base_fd);
		
			// Destuctor
		protected: void fd_close () noexcept;
		public: virtual ~base_fd () noexcept;
		
			// File descriptor accessor
		fd_t get_fd () const { return fd; }
			// Preserve the inode from future automatic actions
		void set_preserved () { shd->preserve_fd = true; }
		
			// fcntl()
/*		#warning TO DO : F_SETOWN/F_GETSIG/F_SETSIG*/
		class fcntl_fl : public socketxx::flags {
		private:
			fd_t fd; fcntl_fl (fd_t fd) : fd(fd) {}
			friend base_fd;
			_SOCKETXX_FLAGS_COMMON(fcntl_fl);
		};
		fcntl_fl fcntl_flags () _SOCKETXX_UNIX_IMPL({ return fcntl_fl(fd); })
		
			// File descriptor type
		mode_t fd_type () const _SOCKETXX_WIN_DELETE; // Cannot be possible with Windows HANDLEs
		
/*			// Aggregation
		void cork ()   { }
		void flush ()  { }
		#warning TO DO : flush() before of after last write() ?*/
		
		// Common I/O routines
	protected:
			// Write
		void _o (const void* d, size_t len) _SOCKETXX_WIN_DELETE; // Normal write
		void _o_flags (const void* d, size_t len, int flags) _SOCKETXX_UNIX_IMPL({ _o(d, len); }) // Not applicable for simple fd, only for sockets !
		
			// Read
		size_t _i (void* d, size_t maxlen) _SOCKETXX_WIN_DELETE; // Normal read, readed data's size is not guaranteed (min 1, max maxlen)
		void _i_fixsize (void* d, size_t len) _SOCKETXX_WIN_DELETE; // Strict read : returns only if [len] data is readed (can block for a very long time : timeout is not strict, it _can_ be reseted each time data is received; use various methods)
		
		public: struct _io_fncts { typedef size_t (socketxx::base_fd::* i_fnct) (void *, size_t); typedef void (socketxx::base_fd::* o_fnct) (const void *, size_t); i_fnct i; o_fnct o; };
		protected: virtual _io_fncts _get_io_fncts () { return _io_fncts({ &base_fd::_i, &base_fd::_o }); }
		
	};
	
		///------ Base class for stream sockets ------///
/*	#warning TO DO : linux http://nick-black.com/dankwiki/index.php/Network_servers : splice() and tee(), signalfd(), sendfile(), aio_...(), epool(), pool(), kqueue()...*/
	class base_socket : public base_fd {
	protected:
		
			// Create a new socket
		base_socket (sa_family_t af) : base_fd() { fd = ::socket((sa_family_t)af, SOCK_STREAM, 0); _SOCKETXX_IS_INVALID_SOCKET(fd) throw socketxx::bad_socket_error(bad_socket_error::SOCKET, false); }
			// Private initialization
		base_socket (bool autoclose_handle, socket_t handle) : base_fd(autoclose_handle, handle) {}  // Don't forget to check file descriptor
			// Default
		base_socket () = delete;
		
	public:
		
			// Public constructors with already created socket
		base_socket (socket_t handle, bool autoclose_handle) : base_fd(autoclose_handle, handle) { _SOCKETXX_IS_INVALID_SOCKET(fd) throw socketxx::bad_socket_error(bad_socket_error::SOCKET, true); }
			// Copy constuctor
		base_socket (const base_socket& other) : base_fd(other) {}
			// Construct from base_fd : underlying MUST be a socket
		base_socket (const base_fd& base) : base_fd(base) {}
		
			// Destuctor
		protected: void fd_close () noexcept;
		public: virtual ~base_socket () noexcept;
		
			// Socket accessor
		operator socket_t () const { return fd; }
		socket_t get_fd () const { return fd; }
		
			// setsockopt() and getsockopt()
		static int  _getopt_sock_int  (socket_t fd, int flag);
		static void _setopt_sock_bool (socket_t fd, int flag, bool b);
		static void _setopt_sock      (socket_t fd, int flag, void* d, size_t s);
		static size_t _getopt_sock    (socket_t fd, int flag, void* d, size_t s);
/*		#warning TO DO (SOL_SOCKET level) : SO_NOSIGPIPE, SO_PRIORITY, SO_OOBINLINE, SO_KEEPALIVE, SO_MARK, SO_BINDTODEVICE ?, SO_RCVBUF, SO_RCVLOWAT+SO_SNDLOWAT (erm, 1 by default, which is right), SO_RCVTIMEO ofc, SO_SNDBUF, SO_TIMESTAMP ?*/
		void set_read_timeout (timeval timeout)  { this->_setopt_sock(fd, SO_RCVTIMEO, &timeout, sizeof(timeval)); }
		timeval get_read_timeout ()  { timeval tm = NULL_TIMEVAL; this->_getopt_sock(fd, SO_RCVTIMEO, &tm, sizeof(timeval)); return tm; }
		
			// ioctl()
/*		#warning TO DO : FIONREAD, SIOCSPGRP ?, SIOCGSTAMP ?, SIOCATMARK, FIONBIO (if not done by fcntl), http://www.linux-kheops.com/doc/man/manfr/man-html-0.9/man2/ioctl_list.2.html => see sockios.h section */	
		
			// Info struct
		struct addr_info {};
		
		// Common I/O routines
	protected:
			// Send
/*		#warning TO DO : send() flags : MSG_NOSIGNAL, MSG_OOB, MSG_MORE !! yeah !, MSG_DONTWAIT (non-blocking), MSG_WAITALL, MSG_PEEK for replacing MSG_WAITALL ? */
		void _o (const void* d, size_t len);
		void _o_flags (const void* d, size_t len, int flags);

			// Read
		size_t _i (void* d, size_t maxlen);
		void _i_fixsize (void* d, size_t len); // Returns only if [len] data is readed, Use MSG_WAITALL if possible
		
		virtual _io_fncts _get_io_fncts () { return _io_fncts({ (_io_fncts::i_fnct)&base_socket::_i, (_io_fncts::o_fnct)&base_socket::_o }); }
	};
	
		///------ Base class for files ------///
/*	class base_file : public base_fd {
	protected:
			// 
		fd_t fd_read;
		fd_t fd_write;
		
		#warning TO DO : Use it for stdin and co. (declared as `FILE* stdin` on Windows)
		#warning Use fwrite() (stdc) & co. instead of write() & co.
		#warning fcntl() : F_GETLK/F_SETLK/F_SETLKW
	};*/
	
		///------ Base class for pipes ------///
/*	class base_pipe : public base_fd {
		#warning WIN32 IDEA : inline int pipe(fd_t* pair) { return _pipe(pair, WIN_PIPE_SIZE, _O_BINARY); }
		#warning WIN32 IDEA : Anonymous pipes (half duplex) : http://msdn.microsoft.com/en-us/library/windows/desktop/aa365139(v=vs.85).aspx
		#warning WIN32 IDEA : Named pipes (half or full duplex, one or more clients) : http://msdn.microsoft.com/en-us/library/windows/desktop/aa365590(v=vs.85).aspx
		#warning IDEA : http://stackoverflow.com/questions/1583005/is-there-any-difference-between-socketpair-and-pair-of-unnamed-pipes
	};*/
	
}

#endif
