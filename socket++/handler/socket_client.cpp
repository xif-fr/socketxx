#include <socket++/handler/socket_client.hpp>

	// OS headers
#include <sys/select.h>
#include <fcntl.h>

namespace socketxx { namespace end {
	
	client_connect_error::~client_connect_error() noexcept {}
	
		// Private external functions
	namespace _socket_client {
		
			// connect() warper
		void connect (socket_t fd, const sockaddr* addr, socklen_t addrlen) {
			int r;
			r = ::connect(fd, addr, addrlen);
			if (r == -1) throw client_connect_error("Failed to connect client to host");
		}
		
		void connect_timeout (socket_t fd, base_fd::fcntl_fl fnctl_flags, const sockaddr* addr, socklen_t addrlen, timeval timeout) {
			int r;
			fnctl_flags |= O_NONBLOCK;
			r = ::connect(fd, addr, addrlen);
			fnctl_flags &= ~O_NONBLOCK;
			if (r == -1) {
				if (errno != EINPROGRESS) throw client_connect_error("Failed to connect client to host");
				fd_set set;
			redo:
				FD_ZERO(&set);
				FD_SET(fd, &set);
				r = ::select(fd+1, NULL, &set, NULL, &timeout);
				if (r == 0) {
					errno = ETIMEDOUT;
					throw client_connect_error("Can't connect to host");
				}
				else if (r > 0) {
					r = base_socket::_getopt_sock_int(fd, SO_ERROR);
					if (r != 0) {
						errno = r;
						throw client_connect_error("Error while connecting to host");
					}
					return;
				} else {
					if (errno == EINTR) goto redo;
					throw client_connect_error("Error while connecting to host");
				}
			}
			return;
		}
	}
	
}}
