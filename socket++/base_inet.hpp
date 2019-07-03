#ifndef SOCKET_XX_BASE_INET_H
#define SOCKET_XX_BASE_INET_H

	// BaseIO
#include <socket++/base_io.hpp>

	// OS headers
#include <netinet/in.h>
#include <arpa/inet.h>

	// General headers
#include <functional>
#include <stdlib.h>

	// To be coherent with in6addr_any
extern const struct in_addr inaddr_any;

namespace socketxx {
	
		// DNS resolving error
	class dns_resolve_error : public socketxx::classic_error {
	public:
		std::string failed_hostname;
		dns_resolve_error (std::string host) noexcept : classic_error(), failed_hostname(host) {}
		virtual ~dns_resolve_error() noexcept {}
	protected:
		virtual std::string descr () const;
	};
	
		///------ Base class for internet TCP IPv4 sockets ------///
	class base_netsock : public base_socket {
	public:
		
			// Defs
		typedef sockaddr_in sockaddr_type;
		static const sa_family_t addr_family = AF_INET;
		
	protected:
		
			// Create a new TCP socket
		base_netsock () : base_socket((sa_family_t)AF_INET) {}
			// Private initialization
		base_netsock (bool autoclose_handle, socket_t handle) : base_socket(autoclose_handle, handle) {}  // Don't forget to check file descriptor
		
	public:
		
			// Contructor from base_socket - underlying socket must have AF_INET family
		base_netsock (const socketxx::base_socket& o) : base_socket(o) {}
		
			// Destuctor
		virtual ~base_netsock () noexcept {}
		
			// Opts
/*		#warning TO DO (IPPROTO_TCP level) : TCP_DEFER_ACCEPT ??, TCP_KEEPCNT - TCP_KEEPIDLE - TCP_KEEPINTVL, TCP_MAXSEG, TCP_QUICKACK & TCP_SYNCNT ?
		#warning TO DO ? : Crazy IP flags (IPPROTO_IP level) : Multicast (yeah...) > IP_ADD_MEMBERSHIP+IP_ADD_SOURCE_MEMBERSHIP+IP_MULTICAST_IF+..., IP_FREEBIND ?, IP_TOS ?, IP_TTL ? */
		
			// Info struct
	protected: 
		
		struct _addrt { 
			sockaddr_in addr; socklen_t len; 
			void use (_addr_use_type_t type, socketxx::base_netsock& sock) { }
			void unuse (_addr_use_type_t type, socketxx::base_netsock& sock) { }
		};
		friend struct _addrt;
		
	public:
		
		struct addr_info {
		private:
			friend class base_netsock;
			const sockaddr_in addr;
			const socklen_t addrlen;
		public:
			addr_info (_addrt _addr) : addr(_addr.addr), addrlen(_addr.len) {}
			_addrt _getaddr () { return _addrt({addr,addrlen}); }
			addr_info (const addr_info& o) = default;
			addr_info& operator= (const addr_info& o) { this->~addr_info(); new(this) addr_info(o); return *this; }
				// Create addr struct from ip address and port. For server binding to any ip, use inaddr_any
			addr_info (in_addr ip, in_port_t port);
				// Create addr from hostname resolving
			addr_info (const char* hostname, in_port_t port);
			addr_info (const char* hostname, std::function<in_port_t()> port_f);
				// Create addr from standardized string "(HOSTNAME|IPADDR)[:PORT]".
			addr_info (in_port_t default_port, std::string addr_str);
				// IP addr to string representation
			static std::string addr2str (in_addr addr);
			static std::string addr2str (in_addr_t addr) { return addr_info::addr2str(::in_addr{addr}); }
				// Getters
			in_addr get_ip_addr () const { return addr.sin_addr; }
			std::string get_ip_str () const { return addr_info::addr2str(addr.sin_addr); }
			in_port_t get_port () const { return ntohs(addr.sin_port); }
		};
		
	};
	
		/// Convert IPs
	// Invalid IP string exception
	class bad_addr_error : public socketxx::error {
	public:
		enum err_type { BAD_IP, BAD_IP6, BAD_ADDR };
	protected:
		err_type _t;
	public:
		bad_addr_error (err_type t) noexcept : _t(t), socketxx::error("Bad address") {}
		bad_addr_error (err_type t, const char* what) noexcept : _t(t), socketxx::error(what) {}
	};
	
	// Convert IP from string format to binary format
	in_addr inline IP (const char* ip) {
		in_addr addr;
		if (::inet_pton(AF_INET, ip, &addr) != 1) throw socketxx::bad_addr_error(bad_addr_error::BAD_IP, "Bad litteral IPv4 addr");
		return addr;
	}
	in6_addr inline IPv6 (const char* ipv6) {
		in6_addr addr;
		if (::inet_pton(AF_INET6, ipv6, &addr) != 1) throw socketxx::bad_addr_error(bad_addr_error::BAD_IP6, "Bad litteral IPv6 addr");
		return addr;
	}
	
}

	/// Literals for IPs
in_addr inline operator "" _IP (const char* ip, size_t) { return socketxx::IP(ip); }
in6_addr inline operator "" _IPv6 (const char* ipv6, size_t) {return socketxx::IPv6(ipv6); }

#endif
