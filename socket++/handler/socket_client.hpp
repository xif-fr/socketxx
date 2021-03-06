#ifndef SOCKET_XX_CLIENT_H
#define SOCKET_XX_CLIENT_H

	// BaseIO
#include <socket++/base_io.hpp>

	// General headers
#include <type_traits>

namespace socketxx { namespace end {
	
		// Private external functions
	namespace _socket_client {
		
			// connect() warpers
		void connect (socket_t fd, const sockaddr* addr, socklen_t addrlen);
		void connect_timeout (socket_t fd, base_fd::fcntl_fl fnctl_flags, const sockaddr* addr, socklen_t addrlen, timeval timeout);
		
	}
	
		/// Exceptions
		// connect error
	class client_connect_error : public socketxx::classic_error {
	protected:
		const char* _what;
		virtual std::string descr () const { return std::string(_what) + this->classic_error::errno_str(); }
	public:
		client_connect_error (const char* what) noexcept : classic_error(), _what(what) {}
		virtual ~client_connect_error() noexcept;
	};
	
		// Enabling socket_client<base_io> only for socketxx::base_socket derivatives
	template <typename socket_base, typename = typename std::enable_if<std::is_base_of<socketxx::base_socket, socket_base>::value>::type>
		class socket_client;
	
	/***** Client-side ending (outcoming socket) *****
	 *
	 *  Simply connects to a distant listening server-side ending.
	 */
	template <typename socket_base>
	class socket_client<socket_base> : public socket_base {
	private:
			// Forbidden constructors
		socket_client () = delete;
		socket_client (const socket_base& o) = delete;
		
	public:
			// Constructor from addr_info
		socket_client (typename socket_base::addr_info addr, timeval max_wait = TIMEOUT_INF); // max_wait is used only for connect timeout
	};
	
	template <typename socket_base> 
	socket_client<socket_base>::socket_client (typename socket_base::addr_info addr, timeval max_wait) : socket_base() {
		auto _addr = addr._getaddr();
		_addr.use(_addr_use_type_t::CLIENT, *this);
		if (max_wait == TIMEOUT_INF)
			_socket_client::connect(socket_base::fd, (const sockaddr*)&_addr.addr, _addr.len);
		else 
			_socket_client::connect_timeout(socket_base::fd, this->base_fd::fcntl_flags(), (const sockaddr*)&_addr.addr, _addr.len, max_wait);
	}
	
}}
	
#endif
