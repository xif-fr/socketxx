#include <socket++/base_unixsock.hpp>
#include <unistd.h>
#include <string.h>

namespace socketxx {
	
	/************* BaseUnixSocket Implementation *************/
	
	inline sockaddr_un _build_unixsock_addr (const char* path) {
		sockaddr_un addr;
		addr.sun_family = AF_UNIX;
		size_t path_len;
		if ((path_len = ::strlen(path)) >= sizeof(addr.sun_path)-1)
			throw socketxx::error("Path is too long for UNIX socket !");
		::strcpy(addr.sun_path, path);
		addr.sun_path[path_len] = '\0';
		return addr;
	}
	
	base_unixsock::addr_info::addr_info (const char* path) : addr(socketxx::_build_unixsock_addr(path)), addrlen((socklen_t)(offsetof(sockaddr_un,sun_path)+::strlen(addr.sun_path))) {}
	
	std::pair<base_unixsock,base_unixsock> base_unixsock::create_socket_pair () {
		int r;
		socket_t p[2];
		r = ::socketpair((sa_family_t)AF_UNIX, SOCK_STREAM, 0, p);
		if (r == SOCKET_ERROR)
			throw socketxx::other_error("can't create socket pair", true);
		return std::pair<base_unixsock,base_unixsock>( base_unixsock(true,p[0]), base_unixsock(true,p[1]) );
	}
	
	void base_unixsock::autodel_sock_file (sockaddr_un& addr) {
		autodel_path = new char[::strlen(addr.sun_path)+1];
		::strcpy(autodel_path, addr.sun_path);
	}
	
	void base_unixsock::del_sock_file () {
		if (autodel_path != NULL) {
			base_socket::fd_close();
			::unlink(autodel_path);
			delete[] autodel_path;
			autodel_path = NULL;
		}
	}
	
	base_unixsock::~base_unixsock () noexcept {
		REFCXX_WILL_DESTRUCT(base_fd) {
			base_socket::fd_close();
			if (autodel_path != NULL) {
				::unlink(autodel_path);
				delete[] autodel_path;
			}
		}
	}
	
}
