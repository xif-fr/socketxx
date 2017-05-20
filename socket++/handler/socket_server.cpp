#include <socket++/handler/socket_server.hpp>

	// OS headers
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>

namespace socketxx { namespace end {
	
	namespace _socket_server {

			/** -------------- Socket server -------------- **/
		
			// Start listening : create, bind, and put in listening state
		void _server_launch (socket_t sock, const sockaddr* addr, size_t addrlen, u_int listen_max, bool reuse) {
			int r;
			if (reuse) {
			#ifdef SO_REUSEADDR
				base_socket::_setopt_sock_bool(sock, SO_REUSEADDR, true);
			#endif
			}
			r = ::bind(sock, addr, (socklen_t)addrlen);
			if (r == -1) throw server_launch_error(server_launch_error::BIND_ERR);
			r = ::listen(sock, (int)listen_max);
			if (r == -1) throw server_launch_error(server_launch_error::LISTEN_ERR);
		}
		
			// Warper for accept() : return the new client's socket
		socket_t _server_accept (socket_t sock, sockaddr* addr, socklen_t* addrlen) {
			socket_t cli_sock = ::accept(sock, addr, addrlen);
			if (cli_sock == -1) throw server_pool_error(server_pool_error::ACCEPT_ERR);
			return cli_sock;
		}
		
			// Create client thread
		pthread_t _server_cli_new_thread (void*(*thread_fnct)(server_thread_data*), void* new_client_ptr) {
			pthread_t new_thread;
			server_thread_data* new_thread_data = new server_thread_data({new_client_ptr});
			::pthread_create(&new_thread, NULL, (void*(*)(void*))thread_fnct, new_thread_data);
			return new_thread;
		}
		
			/** -------------- Pools -------------- **/
		
			// Warpers for select()
			// Return on `fd1` activity, throw a `stop_exception` on `fd2` activity (fd2 priority)
			// Ignore `fd2` if INVALID_SOCKET
		void _select_throw_stop (fd_t fd1, fd_t fd2, timeval timeout, bool ignsig) {
			fd_set select_set;
			int r_sel;
			fd_t maxfd = (fd1 > fd2) ? fd1+1 : fd2+1;
		select_redo:
			FD_ZERO(&select_set);
			FD_SET(fd1, &select_set);
			if (fd2 != SOCKETXX_INVALID_HANDLE) FD_SET(fd2, &select_set);
			r_sel = ::select(maxfd, &select_set, NULL, NULL, (timeout==TIMEOUT_INF)?NULL:&timeout);
			if (r_sel == -1) {
				if (errno == EINTR && ignsig) goto select_redo;
				throw server_pool_error(server_pool_error::SELECT_ERR);
			}
			if (r_sel == 0) throw socketxx::timeout_event();
			if (fd2 != SOCKETXX_INVALID_HANDLE and FD_ISSET(fd2, &select_set)) {
				throw socketxx::stop_event(fd2);
			}
			return;
		}
		
			// Warpers for select()
			// Throw a `stop_exception` on any fd activity in `fds` (first fd in `fds` priority)
		void _select_throw_stop (fd_t fd1, std::vector<fd_t>& fds, timeval timeout, bool ignsig) { 
			fd_set select_set;
			int r_sel;
			fd_t maxfd = fd1;
		select_redo:
			FD_ZERO(&select_set);
			FD_SET(fd1, &select_set);
			for (fd_t fd_monitor : fds) {
				FD_SET(fd_monitor, &select_set);
				if (fd_monitor > maxfd) maxfd = fd_monitor;
			}
			r_sel = ::select(maxfd+1, &select_set, NULL, NULL, (timeout==TIMEOUT_INF)?NULL:&timeout);
			if (r_sel == -1) {
				if (errno == EINTR && ignsig) goto select_redo;
				throw server_pool_error(server_pool_error::SELECT_ERR);
			}
			if (r_sel == 0) throw socketxx::timeout_event();
			for (fd_t fd_monitor : fds) {
				if (FD_ISSET(fd_monitor, &select_set)) {
					throw socketxx::stop_event(fd_monitor);
				}
			}
			return;
		}
		
			// Warpers for select()
			// Simple and transparent warper for select
		uint _select (int maxfd, fd_set* set, timeval timeout) {
			timeval tm;
			int r_sel;
		select_redo:
			tm = timeout;
			r_sel = ::select(maxfd+1, set, NULL, NULL, (tm==TIMEOUT_INF)?NULL:&tm);
			if (r_sel == -1) {
				if (errno == EINTR) goto select_redo;
				throw server_pool_error(server_pool_error::SELECT_ERR);
			}
			if (r_sel == 0) throw socketxx::timeout_event();
			return (uint)r_sel;
		}
		
		const std::logic_error bad_state("socket server : Bad listening state");
		
	}
	
		/** -------------- Exceptions -------------- **/
	
	std::string server_launch_error::descr() const {
		std::string descr = "socket server : ";
		switch (type) {
			case BIND_ERR: descr += "Port/address binding"; break;
			case LISTEN_ERR: descr += "Listening"; break;
		}
		descr += " failed";
		return descr;
	}
	
	std::string server_pool_error::descr() const {
		std::string descr = "socket server pooling : ";
		switch (type) {
			case SELECT_ERR: descr += "select() error"; break;
			case ACCEPT_ERR: descr += "accept() error"; break;
		}
		return descr;
	}
	
}}

#include <socket++/io/simple_socket.hpp>
