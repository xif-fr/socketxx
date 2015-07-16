#ifndef SOCKET_XX_BASE_UNIXSOCK_H
#define SOCKET_XX_BASE_UNIXSOCK_H

	// BaseIO
#include <socket++/base_io.hpp>

	// OS headers
#include <sys/un.h>

	// General headers
#include <utility>

namespace socketxx {
	
		///------ Base class for internet UNIX sockets ------///
	class base_unixsock : public base_socket {
	public:
		
			// Defs
		typedef sockaddr_un sockaddr_type;
		static const sa_family_t addr_family = AF_UNIX;
		
	protected:
		
			// Create a new TCP socket
		base_unixsock () : base_socket((sa_family_t)AF_UNIX) {}
			// Private initialization
		base_unixsock (bool autoclose_handle, socket_t handle) : base_socket(autoclose_handle, handle) {}  // Don't forget to check file descriptor
		
	public:
		
			// Contructor from base_socket - underlying socket must have AF_UNIX family
		base_unixsock (const socketxx::base_socket& o) : base_socket(o) {}
		
			// Destuctor
		virtual ~base_unixsock () noexcept;
		
			// Opts
/*		#warning TO DO : SCM_RIGHTS, SCM_CREDENTIALS, SO_PASSCRED */
		
			// Info struct
	protected: 
		
		char* autodel_path = NULL;
		void autodel_sock_file (sockaddr_un& addr);
		void del_sock_file ();
		
		struct _addrt { 
			sockaddr_un addr; socklen_t len; 
			void use (_addr_use_type_t type, socketxx::base_unixsock& sock) {
				if (type == _addr_use_type_t::SERVER) sock.autodel_sock_file(addr); // Autodelete socket file for server use
			}
			void unuse (_addr_use_type_t type, socketxx::base_unixsock& sock) {
				if (type == _addr_use_type_t::SERVER) sock.del_sock_file(); // Delete socket file for server use
			}
		};
		friend struct _addrt;
		
	public:
		
		struct addr_info {
		private:
			const sockaddr_un addr;
			const socklen_t addrlen;
		public:
			addr_info (_addrt _addr) : addr(_addr.addr), addrlen(_addr.len) {}
			_addrt _getaddr () { return _addrt({addr,addrlen}); }
			addr_info (const addr_info& o) = default;
			addr_info& operator= (const addr_info& o) { this->~addr_info(); new(this) addr_info(o); return *this; }
				// Create addr struct. With server-side, will create a new socket file if don't exists (or throw an EINVAL if exists)
			addr_info (const char* path);
				// Getters
			std::string get_path () const { return std::string(addr.sun_path, sizeof(addr.sun_path)-(sizeof(sockaddr_un)-addrlen)); }
		};
		
	public:
		
			// Create unamed socket pair
		static std::pair<base_unixsock,base_unixsock> create_socket_pair ();
	};
	
}

#endif
