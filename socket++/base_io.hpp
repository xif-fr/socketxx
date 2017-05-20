/**********************************************************************
 * Xif Socket++ Networking Library
 **********************************************************************
 * Copyright Félix Faisant 2012 - 2016
 * Software under GNU LGPL http://www.gnu.org/licenses/lgpl.html
 * For bug reports, help, or improvement requests, please mail at Félix Faisant <xcodexif@xif.fr>
 **********************************************************************
 * Socket++ is a light and modular C++ warper library for POSIX Input/Output systems,
 *  especially TCP/IP sockets
 *
 * The interface is composed of three layers : BaseIO classes, IO protocols, and Connection Handlers.
 * Full description of these classes can be found in respective headers.
 *
 * BaseIO classes are RAII objects for holding and manipulating underlying ressources, like 
 *  file descriptors, files, sockets, Windows HANDLEs... They are responsible for the ressources
 *  and must implement some basic I/O methods for reading and writing. They are also responsible
 *  for ressource's flags/options/special operations. There may be a BaseIO for any thing in which
 *  we can write/send and (or?) read/receive data.
 * Shipped BaseIO classes :
 *    - BaseFD : root handler for UNIX file descriptors / Windows HANDLEs
 *    - BaseSocket : handler for sockets. Subclasses propose an ::addr_info struct for a certain AF.
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
 * Most objects are reference-counted, transparently. You have just to remember that 
 *  you are free to copy socket objects. There are no pointers or object handlers.
 *
 * -== To Do : IPv6 full support - P2P Mode - Socket states ==-
 *
 **********************************************************************/

#ifndef SOCKET_XX_BASE_H
#define SOCKET_XX_BASE_H

	// Global definitions :
/* - fd_t, socket_t
 * - endianness
 * - socketxx::auto_bdata
 * - socketxx::flags
 * - socketxx::event,error exceptions */
#include <socket++/defs.hpp>

	// OS headers
#include <sys/socket.h>
#include <string>

namespace socketxx {
	
		// Addr use types
	enum class _addr_use_type_t {
		SERVER,
		SERVER_CLI,
		CLIENT,
	};
	
	/** ------ Event Exceptions ------ **/
		
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
	
	/** ------ Error Exceptions ------ **/
	
		// Generic classic system or socket exception, using `errno`
	class classic_error : virtual public socketxx::error {
	public:
		int std_errno;
	protected:
		virtual std::string descr () const = 0;
		std::string errno_str () const;
		classic_error (int err) noexcept : error(), std_errno(err) {}
		explicit classic_error () noexcept : error(), std_errno(errno) {}
	public:
		virtual ~classic_error() noexcept { errno_reset; }
	};
	
		// In-Out socket exception
	class io_error : virtual public socketxx::classic_error {
	public:
		enum _type { READ = 0, WRITE = 1 } err_type;
		int ret;
		io_error (ssize_t ret, _type t) noexcept : err_type(t), ret((int)ret), classic_error() {}  // For read/write ops
		io_error (_type t) noexcept : err_type(t), ret(-1), classic_error() {}
		bool is_connection_closed () const noexcept { return ret == 0; }	// `false` do not ensure that the connection is still opened !
		bool is_timeout_error () const noexcept { return std_errno == ETIMEDOUT or std_errno == EAGAIN; }
	protected:
		virtual std::string descr () const;
	public:
		virtual ~io_error() noexcept {}
	};
	
		// Other socket or syscall error
	class other_error : public socketxx::classic_error {
	protected:
		std::string _what;
		virtual std::string descr () const { return _what + this->classic_error::errno_str(); }
	public:
		other_error (std::string what) noexcept : classic_error(), _what(what) {}
		virtual ~other_error() noexcept {}
	};
	
		///-------------------------------------------------///
		///------ Base class for any file descriptors ------///
	class base_fd {
	protected:
			// Using refcounting
		REFCXX_REFCOUNTED(base_fd);
		
			// The file descriptor
		fd_t fd;
			// Shared socket settings
		struct _shrd_data {
			// Autoclose file descriptor
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
		base_fd (fd_t handle) : REFCXX_CONSTRUCTOR(base_fd), fd(handle), shd(new _shrd_data()) {}
		base_fd (bool autoclose_handle, fd_t handle) : REFCXX_CONSTRUCTOR(base_fd), fd(handle), shd(new _shrd_data(autoclose_handle)) {}  // Don't forget to check file descriptor
		static void check_fd (fd_t);
		
	public:
			// Public constructors with already created file descriptor
		#define SOCKETXX_AUTO_CLOSE (bool)true
		#define SOCKETXX_MANUAL_FD (bool)false
		base_fd (fd_t handle, bool autoclose_handle) : REFCXX_CONSTRUCTOR(base_fd), fd(handle), shd(new _shrd_data(autoclose_handle)) { base_fd::check_fd(handle); }
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
		fcntl_fl fcntl_flags () { return fcntl_fl(fd); }
		
			// File descriptor type
		mode_t fd_type () const;
		
			// Data ready to be read
		bool i_avail ();
		
/*			// Aggregation
		void cork ()   { }
		void flush ()  { }
		#warning TO DO : flush() before or after last write() ?*/
		
		// Common I/O routines
	protected:
			// Write
		void _o (const void* d, size_t len); // Normal write
		void _o_flags (const void* d, size_t len, int flags) { _o(d, len); } // Not applicable for simple fd, only for sockets !
		
			// Read
		size_t _i (void* d, size_t maxlen); // Normal read : read data's size is not guaranteed (min 1, max maxlen)
		void _i_fixsize (void* d, size_t len); // Strict read : returns only if [len] data is read; timeout is _not_ strict, it is reset each time data is received
		
		public: struct _io_fncts { typedef size_t (socketxx::base_fd::* i_fnct) (void *, size_t); typedef void (socketxx::base_fd::* o_fnct) (const void *, size_t); i_fnct i; o_fnct o; };
		protected: virtual _io_fncts _get_io_fncts () { return _io_fncts({ &base_fd::_i, &base_fd::_o }); }
		
	};
	
		///----------------------------------///
		///------ Base class for pipes ------///
	
/* TO DO : Template class read/write mode, IO types have to be updated for */

	namespace _base_pipe {
		extern const std::logic_error badend_w, badend_r;
		void _pipe_select_wait (fd_t, timeval* tm);
		size_t _i_pipe (fd_t, void* d, size_t maxlen, timeval tm);
		void _ifix_pipe (fd_t, void* d, size_t len, timeval tm);
		void _check_pipe (fd_t, rw_t);
	}
	template <rw_t rw>
	class base_pipe : public base_fd {
	protected:
		
			// Private initialization
		base_pipe (bool autoclose_handle, fd_t handle) : base_fd(autoclose_handle, handle) {}  // Don't forget to check file descriptor
			// Default
		base_pipe () = delete;
		
	public:
		
			// Public constructors with already created socket
		base_pipe (fd_t handle, bool autoclose_handle) : base_fd(autoclose_handle, handle), timeout(TIMEOUT_INF) { _base_pipe::_check_pipe(handle, rw); }
			// Copy constuctor
		base_pipe (const base_pipe<rw>& other) : base_fd(other), timeout(other.timeout) {}
			// Construct from base_fd
		base_pipe (const base_fd& base) : base_fd(base), timeout(TIMEOUT_INF) { _base_pipe::_check_pipe(this->fd, rw); }
		
			// Destructor
		virtual ~base_pipe () {}
		
			// Timeout. Modifications dot not spread across copies. Special values : TIMEOUT_INF, TIMEOUT_NOBLOCK
		timeval timeout;
		void set_read_timeout (timeval tm) { this->timeout = tm; }
		timeval get_read_timeout () const { return timeout; }
		
		// Common I/O routines
	protected:
			// Send
		void _o (const void* d, size_t len) {
			if (rw != rw_t::WRITE) throw _base_pipe::badend_w;
			base_fd::_o(d, len);
		}
		void _o_flags (const void* d, size_t len, int) { _o(d, len); } // Not applicable for pipes
		
			// Read
		size_t _i (void* d, size_t maxlen) {
			if (rw != rw_t::READ) throw _base_pipe::badend_r;
			return _base_pipe::_i_pipe(fd, d, maxlen, timeout);
		}
		void _i_fixsize (void* d, size_t len) { // Strict read : returns only if [len] data is read; timeout _may not_ be strict, it can be reset each time data is received
			if (rw != rw_t::READ) throw _base_pipe::badend_r;
			_base_pipe::_ifix_pipe(fd, d, len, timeout);
		}
		
		virtual _io_fncts _get_io_fncts () { return _io_fncts({ (_io_fncts::i_fnct)&base_pipe::_i, (_io_fncts::o_fnct)&base_pipe::_o }); }
	};
	
		///-------------------------------------------///
		///------ Base class for stream sockets ------///
/*	#warning TO DO : linux http://nick-black.com/dankwiki/index.php/Network_servers : splice() and tee(), signalfd(), sendfile(), aio_...(), epool(), pool(), kqueue()...*/
	class base_socket : public base_fd {
	protected:
		
			// Create a new socket
		static fd_t create_socket (sa_family_t af);
		base_socket (sa_family_t af) : base_fd( base_socket::create_socket(af) ) {}
			// Private initialization
		void check_socket () const;
		base_socket (bool autoclose_handle, socket_t handle) : base_fd(autoclose_handle, handle) {}  // Don't forget to check file descriptor
			// Default
		base_socket () = delete;
		
	public:
		
			// Public constructors with already created socket
		base_socket (socket_t handle, bool autoclose_handle) : base_fd(autoclose_handle, handle) { this->check_socket(); }
			// Copy constuctor
		base_socket (const base_socket& other) : base_fd(other) {}
			// Construct from base_fd : underlying MUST be a socket
		base_socket (const base_fd& base) : base_fd(base) { this->check_socket(); }
		
			// Destuctor
		protected: void fd_close () noexcept;
		public: virtual ~base_socket () noexcept;
		
			// Socket accessor
		operator socket_t () const { return fd; }
		socket_t get_fd () const { return fd; }
		
			// setsockopt() and getsockopt()
		static void _setopt_sock      (socket_t fd, int flag, const void* d, size_t s);
		static void _setopt_sock_bool (socket_t fd, int flag, bool b);
		static size_t _getopt_sock    (socket_t fd, int flag, void* d, size_t s);
		static int  _getopt_sock_int  (socket_t fd, int flag);
/*		#warning TO DO (SOL_SOCKET level) : SO_NOSIGPIPE, SO_PRIORITY, SO_OOBINLINE, SO_KEEPALIVE, SO_MARK, SO_BINDTODEVICE ?, SO_RCVBUF, SO_RCVLOWAT+SO_SNDLOWAT (erm, 1 by default, which is right), SO_RCVTIMEO ofc, SO_SNDBUF, SO_TIMESTAMP ?*/
		void set_read_timeout (timeval timeout);
		timeval get_read_timeout () const;
		
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
		void _i_fixsize (void* d, size_t len); // Returns only if [len] data is read, Use MSG_WAITALL if possible
		
		virtual _io_fncts _get_io_fncts () { return _io_fncts({ (_io_fncts::i_fnct)&base_socket::_i, (_io_fncts::o_fnct)&base_socket::_o }); }
	};
	
}

#endif
