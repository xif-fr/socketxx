#include <socket++/base_inet.hpp>

	// OS headers
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

	// Other
#include <sstream>
#include <string.h>
#include <xifutils/intstr.hpp>

const struct in_addr inaddr_any = {INADDR_ANY};

namespace socketxx {
	
	/************* BaseTCPSock Implementation *************/
	
	inline in_addr _resolve_hostname (const char* hostname) {
		struct hostent* host_struct = ::gethostbyname(hostname);
		if (host_struct == NULL) throw dns_resolve_error(hostname);
		if (host_struct->h_addr_list[0] == NULL) throw dns_resolve_error(hostname);
		in_addr ip_addr = (*((in_addr**)host_struct->h_addr_list)[0]);
		return ip_addr;
	}
	
	inline sockaddr_in _build_ipsock_addr (in_port_t port, in_addr ip) {
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr = ip;
		addr.sin_port = htons(port);
		return addr;
	}
	
	inline sockaddr_in _build_ipsock_addr_from_str (in_port_t default_port, std::string& addr_str) {
		int r;
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		if (::isdigit(addr_str[addr_str.length()-1]))
			for (size_t i = addr_str.size()-1; i != 0; i--) {
				if (addr_str[i-1] == ':') {
					int port = ::atoi( addr_str.substr(i, std::string::npos).c_str() );
					if (port < 0 or port > UINT16_MAX) throw socketxx::bad_addr_error(bad_addr_error::BAD_ADDR, "Bad addr : port number out of range");
					addr.sin_port = htons(port);
					addr_str.resize(i-1);
					goto port_ok;
				}
			}
		addr.sin_port = htons(default_port);
	port_ok:
		if (addr_str.empty()) 
			throw socketxx::bad_addr_error(bad_addr_error::BAD_ADDR, "Bad addr : empty hostname");
		r = ::inet_pton(AF_INET, addr_str.c_str(), &addr.sin_addr);
		if (r == 1) 
			return addr;
		for (size_t i = 0; i < addr_str.length(); ++i) {
			if (not (::isalnum(addr_str[i]) || addr_str[i] == '-' || addr_str[i] == '.'))
				throw socketxx::bad_addr_error(bad_addr_error::BAD_ADDR, "Bad addr : bad hostname");
		}
		addr.sin_addr = socketxx::_resolve_hostname(addr_str.c_str());
		return addr;
	}
	
	base_netsock::addr_info::addr_info (in_addr ip, in_port_t port) : addr(socketxx::_build_ipsock_addr(port,ip)), addrlen(sizeof(sockaddr_in)) {}

	base_netsock::addr_info::addr_info (const char* hostname, in_port_t port) : addr_info(socketxx::_resolve_hostname(hostname), port) {}
	
	base_netsock::addr_info::addr_info (in_port_t default_port, std::string addr_str) : addr(socketxx::_build_ipsock_addr_from_str(default_port,addr_str)), addrlen(sizeof(sockaddr_in)) {}
	
	std::string base_netsock::addr_info::addr2str (in_addr addr) {
		const uint8_t* b = (const u_int8_t*)&addr.s_addr;
		return ::ixtoa(b[0]) + '.' + ::ixtoa(b[1]) + '.' + ::ixtoa(b[2]) + '.' + ::ixtoa(b[3]);;
	}
	
}
