#include <socket++/base_ssl.hpp>

	// General headers
#include <sstream>
#include <errno.h>

#ifdef XIF_USE_SSL

#warning TO DO : templatize base_ssl for all socket types

_socketxx_openssl_init _socketxx_openssl_data;

namespace socketxx {
	
	/************* BaseSSL Implementation *************/
	
		/// OpenSSL error

	std::string socketxx::ssl_error::_str() const noexcept {
		std::ostringstream descr;
		const char* when = NULL;
		switch (t) {
			case READ: when = "reading data"; break;
			case WRITE: when = "writing data"; break;
			case START: when = "starting SSL mode"; break;
			case STOP: when = "stopping SSL mode"; break;
		}
		if (ssl_sock == NULL) {
			descr << "SSL error during " << when;
		} else {
			int ssl_error = SSL_get_error(ssl_sock, ssl_r);
			if (ssl_error == SSL_ERROR_SSL) 
				descr << "Internal OpenSSL error during " << when;
			else if (ssl_error == SSL_ERROR_SYSCALL) 
				descr << "Underlying Socket error in SSL during " << when;
			else if (ssl_error == SSL_ERROR_WANT_READ || ssl_error ==  SSL_ERROR_WANT_WRITE) 
				descr << "IO SSL socket error (EWOULDBLOCK) during " << when;
			else if (ssl_error == SSL_ERROR_WANT_CONNECT || ssl_error ==  SSL_ERROR_WANT_ACCEPT) 
				descr << "SSL handshake error during " << when;
			else if (ssl_error == SSL_ERROR_ZERO_RETURN) 
				descr << "SSL bad connection during " << when;
			else 
				descr << "SSL error during " << when;
		}
		unsigned long err_error = ERR_get_error();
		if (err_error == 0) {
			if (ssl_r == SOCKET_ERROR) 
				descr << " ! 0 = Underlying socket error = " << errno << " = " << strerror(errno);
			else 
				descr << " ! (an EOF was observed that violates the SSL protocol)";
		} else {
			descr << " ! " << err_error << " = " << ERR_reason_error_string(err_error) << " (in " << ERR_func_error_string(err_error) << ")";
			while ((err_error = ERR_get_error()) != 0) 
				descr << " > " << err_error << " = " << ERR_reason_error_string(err_error) << " (in " << ERR_func_error_string(err_error) << ")";
		}
		ERR_clear_error();
		return descr.str();
	}
	
		/// New SSL socket
	
	void base_ssl::new_ssl_socket (const SSL_METHOD* method) {
		ssl_ctx = SSL_CTX_new(const_cast<SSL_METHOD*>(method));
		if (ssl_ctx == NULL) 
			throw socketxx::ssl_error(ssl_error::START);
		ssl_sock = SSL_new(ssl_ctx);
		if (ssl_sock == NULL) 
			throw socketxx::ssl_error(ssl_error::START);
		if (!SSL_set_fd(ssl_sock, base_socket::fd)) 
			throw socketxx::ssl_error(ssl_error::START);
	}
	
		/// Start/stop SSL session
	
	void base_ssl::start_ssl () {
		#warning test if already started
		this->new_ssl_socket(TLS_client_method());
		if (SSL_connect(ssl_sock) <= 0) 
			throw socketxx::ssl_error(ssl_error::START);
	}

	void base_ssl::wait_for_ssl () {
		this->new_ssl_socket(TLS_server_method());
		if (SSL_accept(ssl_sock) <= 0) 
			throw socketxx::ssl_error(ssl_error::START);
	}

	void base_ssl::stop_ssl () {
		if (ssl_sock == NULL) throw std::logic_error("can't stop SSL : SSL not started");
		if (SSL_shutdown(ssl_sock) <= 0) 
			throw socketxx::ssl_error(ssl_error::STOP);
		SSL_free(ssl_sock);
		ssl_sock = NULL;
		SSL_CTX_free(ssl_ctx);
		ssl_ctx = NULL;
	}

		/// Read/Write methods
	
	void base_ssl::_o_ssl (const void* d, size_t len) {
		int ret = SSL_write(ssl_sock, d, (int)len);
		if (ret < (int)len) throw socketxx::io_ssl_error(io_error::WRITE, ssl_sock, ret);
		ERR_clear_error();
		errno = 0;
	}

	size_t base_ssl::_i_ssl (void* d, size_t maxlen) {
		int ret = SSL_read(ssl_sock, d, (int)maxlen);
		if (ret < 1) throw socketxx::io_ssl_error(io_error::READ, ssl_sock, ret);
		ERR_clear_error();
		errno = 0;
		return (size_t)ret;
	}

	void base_ssl::_i_fixsize_ssl (void* d, size_t len) {
		size_t r;
		char* data = (char*)d;
		r = this->_i_ssl(data, len);
		if (r < len) {
			size_t rest = len - r;
			data += r;
			while (rest != 0) {
				r = this->_i_ssl(data, len);
				data += r;
				rest -= r;
			}
		}
	}

}

#endif
