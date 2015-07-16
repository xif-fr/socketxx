#ifndef SOCKET_XX_BASE_SSL_H
#define SOCKET_XX_BASE_SSL_H

	// BaseIO, BaseInet
#include <socket++/base_io.hpp>
#include <socket++/base_inet.hpp>

#ifdef XIF_USE_SSL

	// OpenSSL headers
#include <openssl/ssl.h>
#include <openssl/err.h>
struct _socketxx_openssl_init {
	_socketxx_openssl_init() { SSL_library_init(); SSL_load_error_strings(); }
	~_socketxx_openssl_init() { ERR_free_strings(); EVP_cleanup(); CRYPTO_cleanup_all_ex_data(); }
};

namespace socketxx {

		// OpenSSL exception
	class ssl_error : public socketxx::error {
	private:
		std::string _str () const noexcept;
	public:
		enum _type { READ = 0, WRITE = 1, START, STOP } t;
		SSL* ssl_sock;
		int ssl_r;
		ssl_error (_type t, SSL* sockssl = NULL, int ret = SOCKET_ERROR) noexcept : t(t), ssl_sock(sockssl), ssl_r(ret), error() { this->descr = this->_str(); }
	};
	
		// IO SSL error (you can catch it with io_error or ssl_error)
	class io_ssl_error : public io_error, public ssl_error {
	public:
		io_ssl_error(io_error::_type t, SSL* sockssl, int ret) throw() : io_error(t, ret), ssl_error((ssl_error::_type)t, sockssl, ret) { }
	};
	
		///------ Base class for SSL-enabled sockets ------///
	class base_ssl : public base_netsock {
	protected:
		
			// SSL Data
		SSL* ssl_sock;
		SSL_CTX* ssl_ctx;
		
			// Create SSL socket on top of `fd` socket
		void new_ssl_socket (const SSL_METHOD* method);
		
			// Create a new TCP socket
		base_ssl () : base_netsock(), ssl_sock(NULL), ssl_ctx(NULL) {}
			// Private initialization
		base_ssl (bool autoclose_handle, socket_t handle) : base_netsock(autoclose_handle, handle), ssl_sock(NULL), ssl_ctx(NULL) {}  // Don't forget to check file descriptor
			// No copy
		base_ssl (const base_ssl&) = delete;
		
	public:
		
			// Destuctor
		virtual ~base_ssl () noexcept { REFCXX_WILL_DESTRUCT(base_fd) { if (ssl_sock != NULL) try { this->stop_ssl(); } catch (...) {} } }
		
			// SSL Version enum
		enum ssl_version { SSLv3, TLSv1, TLSv1_2 };
		
			// Contructor from base_netsock
		base_ssl (const socketxx::base_netsock& o) : base_netsock(o), ssl_sock(NULL), ssl_ctx(NULL) {}
		
			// SSL connection
		void wait_for_ssl (ssl_version version); // For the initiator who waits the other side to begin the SSL connection (server side typically)
		void start_ssl (ssl_version version); // For the side who really begin the SSL connection (client side typically)
		void stop_ssl (); // For both
		
			// SSL flags
/*		#warning TO DO : flags SSL_set_mode() : SSL_MODE_RELEASE_BUFFERS ?, SSL_MODE_AUTO_RETRY*/
		
		// Private SSL I/O routines
	private:
			// SSL_Write()
		void _o_ssl (const void* d, size_t len);
			// SSL_Read()
		size_t _i_ssl (void* d, size_t maxlen);
		void _i_fixsize_ssl (void* d, size_t len);
		
		// Common I/O routines
	protected:
			// Send
		void _o (const void* d, size_t len) { if (ssl_sock == NULL) base_socket::_o(d, len); else _o_ssl(d, len); }
		void _o_flags (const void* d, size_t len, int flags) { if (ssl_sock == NULL) base_socket::_o_flags(d, len, flags); else _o_ssl(d, len); } // No flags for SSL sockets
		
			// Read
		size_t _i (void* d, size_t maxlen) { if (ssl_sock == NULL) return base_socket::_i(d, maxlen); else return _i_ssl(d, maxlen); }
		void _i_fixsize (void* d, size_t len) { if (ssl_sock == NULL) base_socket::_i_fixsize(d, len); else _i_fixsize_ssl(d, len); }
		
		virtual _io_fncts _get_io_fncts () { return _io_fncts({ (_io_fncts::i_fnct)&base_ssl::_i, (_io_fncts::o_fnct)&base_ssl::_o }); }
	};
	
}

#endif

#endif
