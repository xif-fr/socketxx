#include <socket++/base_io.hpp>

	// General headers
#include <sstream>
#include <errno.h>
#include <string.h>

	// OS headers
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>

namespace socketxx {
	
	/** -------------- BaseFD Implementation -------------- **/
	
		// Flag operations
	int base_fd::fcntl_fl::get () const {
		int flags = ::fcntl(fd, F_GETFL);
		if (flags < 0) throw socketxx::other_error("Error getting file descriptor's flags with fcntl(F_GETFL)");
		return flags;
	}
	void base_fd::fcntl_fl::set (int flags) {
		if (::fcntl(fd, F_SETFL, flags) < 0) throw socketxx::other_error("Error setting file descriptor's flags with fcntl(F_SETFL)");
	}
	base_fd::fcntl_fl::~fcntl_fl () {}
	
		// File desciptor type/validity
	void base_fd::check_fd (fd_t fd) {
		if (fd < 0 or (::fcntl(fd, F_GETFD) == -1 and errno == EBADF))
			throw socketxx::other_error("Invalid file descriptor");
	}
	mode_t base_fd::fd_type () const {
		struct stat statbuf;
		if (::fstat(fd, &statbuf) == -1) throw socketxx::other_error("Error querying file descriptor type with fstat()");
		mode_t fd_type = statbuf.st_mode;
		return (fd_type & S_IFMT); // Returns only the fd type part of fd mode
	}
	
		// Data availability
	bool base_fd::i_avail () {
		fd_set fdset;
		int r;
		timeval tm = {0,0};
		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);
		r = ::select(fd+1, &fdset, NULL, NULL, &tm);
		if (r == -1)
			throw socketxx::io_error(-1, io_error::READ);
		return (r == 1 and FD_ISSET(fd, &fdset));
	}
	
		// Destruction
	void base_fd::fd_close () noexcept {
		if (shd->autoclose and fd != SOCKETXX_INVALID_HANDLE) { // If file descriptor was not closed by child class, use generic `close()`
			::close(fd);
			fd = SOCKETXX_INVALID_HANDLE;
		}
	}
	base_fd::~base_fd () noexcept {
		REFCXX_DESTRUCT(base_fd) {
			fd_close();
			delete shd;
		}
	}
	
	void base_fd::_o (const void* d, size_t len) {
		ssize_t r;
		r = ::write(fd, d, len);
		if (r != (ssize_t)len) throw socketxx::io_error(r, io_error::WRITE);
	}
	
	size_t base_fd::_i (void* d, size_t maxlen) {
		ssize_t r;
		r = ::read(fd, d, maxlen);
		if (r <= 0) throw socketxx::io_error(r, io_error::READ);
		return (size_t)r;
	}
	void base_fd::_i_fixsize (void* d, size_t len) {
		ssize_t r;
		char* data = (char*)d;
		r = ::read(fd, data, len);
		if (r <= 0) throw socketxx::io_error(r, io_error::READ);
		if ((size_t)r < len) {
			size_t rest = len - (size_t)r;
			data += r;
			while (rest != 0) {
				r = ::read(fd, data, len);
				if (r <= 1) throw socketxx::io_error(r, io_error::READ);
				data += r;
				rest -= (size_t)r;
			}
		}
	}
	
		/** -------------- BasePipe Implementation -------------- **/
	
	const std::logic_error _base_pipe::badend_w ("pipe end not writable");
	const std::logic_error _base_pipe::badend_r ("pipe end not readable");
	
	void _base_pipe::_check_pipe (fd_t fd, rw_t rw) {
		struct stat statbuf;
		if (::fstat(fd, &statbuf) == -1)
			throw socketxx::other_error("Error querying file descriptor type with fstat()");
		mode_t fd_mode = statbuf.st_mode;
		if (not S_ISFIFO(fd_mode))
			throw socketxx::error("File descriptor is not a pipe");
		if ((fd_mode & S_IRUSR) != (rw == rw_t::READ))
			throw socketxx::error("Pipe end mode incompatible with specified rw_t");
	}
	
	void _base_pipe::_pipe_select_wait (fd_t fd, timeval* tm) {
		fd_set select_set;
		int r_sel;
	select_redo:
		FD_ZERO(&select_set);
		FD_SET(fd, &select_set);
		r_sel = ::select(fd+1, &select_set, NULL, NULL, tm);
		if (r_sel == -1) {
			if (errno == EINTR) goto select_redo;
			throw socketxx::io_error(-1, io_error::READ);
		}
		if (r_sel == 0) {
			errno = ETIMEDOUT;
			throw socketxx::io_error(-1, io_error::READ);
		}
		return;
	}
	
	size_t _base_pipe::_i_pipe (fd_t fd, void* d, size_t maxlen, timeval tm) {
		if (tm != TIMEOUT_INF)
			_base_pipe::_pipe_select_wait(fd, &tm);
		ssize_t r;
		r = ::read(fd, d, maxlen);
		if (r <= 0) throw socketxx::io_error(r, io_error::READ);
		return (size_t)r;
	}
	void _base_pipe::_ifix_pipe (fd_t fd, void* d, size_t len, timeval tm) {
		ssize_t r;
		char* data = (char*)d;
		while (len != 0) {
			if (tm != TIMEOUT_INF)
				_base_pipe::_pipe_select_wait(fd, &tm);
			r = ::read(fd, data, len);
			if (r <= 0) throw socketxx::io_error(r, io_error::READ);
			data += r;
			len -= (size_t)r;
		}
	}
	
		/** -------------- BaseSocket Implementation -------------- **/
	
		// Socket creation
	fd_t base_socket::create_socket (sa_family_t af) {
		fd_t fd = ::socket((sa_family_t)af, SOCK_STREAM, 0);
		if (fd == -1) throw socketxx::other_error("Failed to create socket");
		return fd;
	}
	void base_socket::check_socket () const {
		mode_t fdmode = this->fd_type();
		if (not S_ISSOCK(fdmode))
			throw socketxx::error("base_socket : underlying fd is not a socket");
	}
	
		// Destruction
	void base_socket::fd_close () noexcept {
		if (shd->autoclose and fd != SOCKETXX_INVALID_HANDLE) {
			if (not shd->preserve_fd) {
				::shutdown(fd, SHUT_RDWR);
			}
			::close(fd);
			fd = SOCKETXX_INVALID_HANDLE;
		}
	}
	base_socket::~base_socket () noexcept {
		REFCXX_WILL_DESTRUCT(base_fd) {
			fd_close();
		}
	}
	
		// Socket options
	int base_socket::_getopt_sock_int (socket_t fd, int flag) {
		int val;
		socklen_t sz = sizeof(val);
		int r = ::getsockopt(fd, SOL_SOCKET, flag, (void*)(&val), &sz);
		if (r == -1) throw socketxx::other_error("getsockopt() error");
		return val;
	}
	size_t base_socket::_getopt_sock (socket_t fd, int flag, void* d, size_t s) {
		socklen_t sz = (socklen_t)s;
		int r = ::getsockopt(fd, SOL_SOCKET, flag, d, &sz);
		if (r == -1) throw socketxx::other_error("getsockopt() error");
		return sz;
	}
	void base_socket::_setopt_sock_bool (socket_t fd, int flag, bool b) {
		int set = (int)b;
		int r = ::setsockopt(fd, SOL_SOCKET, flag, (void*)&set, (socklen_t)sizeof(int));
		if (r == -1) throw socketxx::other_error("setsockopt() error");
	}
	void base_socket::_setopt_sock (socket_t fd, int flag, const void* d, size_t s) {
		int r = ::setsockopt(fd, SOL_SOCKET, flag, d, (socklen_t)s);
		if (r == -1) throw socketxx::other_error("setsockopt() error");
	}
	
		// Timeouts
	void base_socket::set_read_timeout (timeval timeout) {
		#warning TO DO : Implement non blocking calls
		if (timeout == TIMEOUT_NOBLOCK)
			throw socketxx::error("set_read_timeout : non blocking IO ops not supported");
		if (timeout == TIMEOUT_INF)
			timeout = {0,0};
		this->_setopt_sock(fd, SO_RCVTIMEO, &timeout, sizeof(timeval));
	}
	timeval base_socket::get_read_timeout () const {
		timeval tm = {0};
		this->_getopt_sock(fd, SO_RCVTIMEO, &tm, sizeof(timeval));
		if (tm == timeval({0,0}))
			tm = TIMEOUT_INF;
		return tm;
	}
	
		/// Common I/O routines
	
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
	
		/** -------------- Exceptions -------------- **/
	
		// Events
	const char* socketxx::stop_event::what() const noexcept {
		return "socket++ event : interrupted blocking/waiting opperation by monitored file descriptor";
	}
	const char* socketxx::timeout_event::what() const noexcept {
		return "socket++ timeout event";
	}
		// POSIX call errno error
	std::string socketxx::classic_error::errno_str () const {
		std::ostringstream descr;
		descr << " : #" << std_errno << ' ' << ::strerror(std_errno);
		return descr.str();
	}
		// I/O error
	std::string socketxx::io_error::descr () const {
		std::ostringstream descr;
		if (ret > 0) {
			descr << "Incomplete " << (err_type==WRITE ? "write" : "read");
		} else {
			descr << "I/O error while " << (err_type==WRITE ? "writing" : "reading") << " data";
			if (ret == 0)
				descr << " : the connection is probably closed";
			else
				descr << this->classic_error::errno_str();
		}
		return descr.str();
	}
	
}
