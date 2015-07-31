#include <socket++/base_io.hpp>

	// General headers
#include <sstream>
#include <errno.h>
#include <string.h>

	// OS headers
#if defined (_WIN32)
	_socketxx_winsock_init _socketxx_winsock_data;
#else // UNIXs
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/stat.h>
	#include <fcntl.h>
#endif

namespace socketxx {
	
		/************* BaseFD Implementation *************/

#ifndef _WIN32
		/// BaseFD : fcntl()
	int base_fd::fcntl_fl::get () const {
		int flags = ::fcntl(fd, F_GETFL);
		if (flags < 0) throw socketxx::other_error("Error getting file descriptor's flags with fcntl(F_GETFL)",false);
		return flags;
	}
	void base_fd::fcntl_fl::set (int flags) {
		if (::fcntl(fd, F_SETFL, flags) < 0) throw socketxx::other_error("Error setting file descriptor's flags with fcntl(F_SETFL)",false);
	}
	base_fd::fcntl_fl::~fcntl_fl () {}
	
		/// BaseFD : get file descriptor type
	mode_t base_fd::fd_type () const {
		struct stat statbuf;
		if (::fstat(fd, &statbuf) == -1) throw socketxx::other_error("Error querying file descriptor type with fstat()",false);
		mode_t fd_type = statbuf.st_mode;
		return (fd_type & S_IFMT); // Returns only the fd type part of fd mode
	}
#endif
	

		/// BaseFD : Close the file descriptor
	void base_fd::fd_close () noexcept {
		if (shd->autoclose and fd != INVALID_HANDLE) { // If file descriptor was not closed by child class, use generic `close()`
	#ifdef _WIN32
			CloseHandle(fd);
	#else
			::close(fd);
	#endif
			fd = INVALID_HANDLE;
		}
	}
		/// Destructor
	base_fd::~base_fd () noexcept {
		REFCXX_DESTRUCT(base_fd) {
			fd_close();
			delete shd;
		}
	}

#ifndef _WIN32
		/// Common I/O routines (no file descriptors on windows)
		
		// Write
	void base_fd::_o (const void* d, size_t len) { // Normal write
		ssize_t r;
		r = ::write(fd, d, len);
		if (r < (ssize_t)len) throw socketxx::io_error(io_error::WRITE, (r >= 0));
	}
	
		// Read
	size_t base_fd::_i (void* d, size_t maxlen) { // Normal read : read data's size is not guaranteed (min 1, max maxlen)
		ssize_t r;
		r = ::read(fd, d, maxlen);
		if (r < 1) throw socketxx::io_error(io_error::READ);
		return (size_t)r;
	}
	void base_fd::_i_fixsize (void* d, size_t len) { // Strict read : returns only if [len] data is read - timeout is not strict, it is be reseted each time data is received
		ssize_t r;
		char* data = (char*)d;
		r = ::read(fd, data, len);
		if (r < 1) throw socketxx::io_error(io_error::READ);
		if ((size_t)r < len) {
			size_t rest = len - (size_t)r;
			data += r;
			while (rest != 0) {
				r = ::read(fd, data, len);
				if (r < 1) throw socketxx::io_error(io_error::READ);
				data += r;
				rest -= (size_t)r;
			}
		}
	}
#endif
	
		/************* BaseSocket Implementation *************/
	
		/// BaseSocket : Destructor
	void base_socket::fd_close () noexcept {
		if (shd->autoclose and fd != INVALID_HANDLE) {
			if (not shd->autoclose) {
				::shutdown(fd, SHUT_RDWR);
				#warning TO DO : SO_LINGER
			}
	#ifdef _WIN32
			closesocket(fd);
	#else
			::close(fd);
	#endif
			fd = INVALID_HANDLE;
		}
	}
	base_socket::~base_socket () noexcept {
		REFCXX_WILL_DESTRUCT(base_fd) {
			fd_close();
		}
	}
	
		/// Set/Get socket option
	
	int base_socket::_getopt_sock_int (socket_t fd, int flag) {
		int val;
		socklen_t sz = sizeof(val);
		int r = ::getsockopt(fd, SOL_SOCKET, flag, (void*)(&val), &sz);
		if (r == SOCKET_ERROR) throw socketxx::other_error("getsockopt() error", true);
		return val;
	}
	size_t base_socket::_getopt_sock (socket_t fd, int flag, void* d, size_t s) {
		socklen_t sz = (socklen_t)s;
		int r = ::getsockopt(fd, SOL_SOCKET, flag, d, &sz);
		if (r == SOCKET_ERROR) throw socketxx::other_error("getsockopt() error", true);
		return sz;
	}
	void base_socket::_setopt_sock_bool (socket_t fd, int flag, bool b) {
		int set = (int)b;
		int r = ::setsockopt(fd, SOL_SOCKET, flag, (void*)&set, (socklen_t)sizeof(int));
		if (r == SOCKET_ERROR) throw socketxx::other_error("setsockopt() error", true);
	}
	void base_socket::_setopt_sock (socket_t fd, int flag, void* d, size_t s) {
		int r = ::setsockopt(fd, SOL_SOCKET, flag, d, (socklen_t)s);
		if (r == SOCKET_ERROR) throw socketxx::other_error("setsockopt() error", true);
	}
	
		/// Common I/O routines

		// Default send() and recv() flags
	#ifndef MSG_NOSIGNAL
		#define MSG_NOSIGNAL 0
	#endif
	
		// Send
	void base_socket::_o (const void* d, size_t len) { // Normal send()
		ssize_t r;
		r = ::send(fd, d, len, MSG_NOSIGNAL);
		if (r < (ssize_t)len) throw socketxx::io_error(r, io_error::WRITE);
	}
	void base_socket::_o_flags (const void* d, size_t len, int flags) { // send() with flags
		ssize_t r;
		r = ::send(fd, d, len, flags|MSG_NOSIGNAL);
		if (r < (ssize_t)len) throw socketxx::io_error(r, io_error::WRITE);
	}
	
		// Receive
	size_t base_socket::_i (void* d, size_t maxlen) {
		ssize_t r;
		r = ::recv(fd, d, maxlen, MSG_NOSIGNAL);
		if (r < 1) throw socketxx::io_error(r, io_error::READ);
		return (size_t)r;
	}
#ifndef MSG_WAITALL
	void base_socket::_i_fixsize (void* d, size_t len) { // Returns only if [len] data is read
		ssize_t r;
		char* data = (char*)d;
		r = ::recv(fd, data, len, MSG_NOSIGNAL);
		if (r < 1) throw socketxx::io_error(r, io_error::READ);
		if ((size_t)r < len) {
			size_t rest = len - r;
			data += r;
			while (rest != 0) {
				r = ::recv(fd, data, len, MSG_NOSIGNAL);
				if (r < 1) throw socketxx::io_error(r, io_error::READ);
				data += r;
				rest -= r;
			}
		}
	}
#else
	void base_socket::_i_fixsize (void* d, size_t len) { // Returns only if [len] data is read, Use MSG_WAITALL
		ssize_t r;
		r = ::recv(fd, d, len, MSG_NOSIGNAL|MSG_WAITALL);
		if (r < (ssize_t)len) throw socketxx::io_error(r, io_error::READ);
	}
#endif
		
		/************* BasePipe Implementation *************/
	
	
	
	
		/************* BaseFile Implementation *************/
	
	
	

		/************* Exceptions *************/
	//#warning Use FormatMessage() on Windows
	
		/// Blocking opperation interrupted event
	const char* socketxx::stop_event::what() const throw() {
		return "socket++ event : interrupted blocking/waiting opperation by monitored file descriptor";
	}
	
		/// Timeout event
	const char* socketxx::timeout_event::what() const throw() {
		return "socket++ timeout event";
	}
	
		/// Errno error
	std::string socketxx::classic_error::_errno_str (int std_errno) noexcept {
		std::ostringstream descr;
		descr << " : #" << std_errno << ' ' << ::strerror(std_errno);
		return descr.str();
	}
	socketxx::classic_error::~classic_error() noexcept {}
	
		/// IO error
	std::string socketxx::io_error::_str() const noexcept {
		std::ostringstream descr;
		if (this->ret > 0) {
			descr << "Incomplete " << (err_type==WRITE ? "write" : "read");
		} else {
			descr << "I/O error while " << (err_type==WRITE ? "writing" : "reading") << " data";
			if (this->ret == 0) 
				descr << " : the connection is probably closed";
			else
				descr << classic_error::_errno_str(this->std_errno);
		}
		return descr.str();
	}
	
		/// Invalid file descriptor error
	std::string socketxx::bad_socket_error::_str() const noexcept {
		std::ostringstream descr;
		switch (fd_type) {
			case FD: descr << "Handle/File descriptor"; break;
			case PIPE: descr << "Pipe"; break;
			case SOCKET: descr << "Socket"; break;
		}
		if (fd_from_user) 
			descr << " is invalid";
		else
			descr << " could not be created" << classic_error::_errno_str(this->std_errno);
		return descr.str();
	}
	
}
