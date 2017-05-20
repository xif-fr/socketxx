#ifndef SOCKET_XX_HANDLER_SOCKET_SERVER_H
#define SOCKET_XX_HANDLER_SOCKET_SERVER_H

	// BaseIO
#include <socket++/base_io.hpp>

	// General headers
#include <list>
#include <vector>
#include <functional>
#include <stdexcept>

	// Threads
#ifndef XIF_NO_THREADS
	#include <pthread.h>
#endif

	// OS headers
#include <sys/select.h>

namespace socketxx { 
	
		// Return type for pool callbacks
	enum pool_ret_t { POOL_CONTINUE, POOL_QUIT, POOL_RESCAN };
	
	namespace end {
	
		/// Exceptions ///
	
		// Error during server launching (reuse port, bind or listen)
	class server_launch_error : public socketxx::classic_error {
	public:
		enum _type { BIND_ERR, LISTEN_ERR } type;
		server_launch_error(_type t) noexcept : type(t), classic_error() {}
	protected:
		virtual std::string descr () const;
	};
		// accept()/select() error
	class server_pool_error : public socketxx::classic_error {
	public:
		enum _type { SELECT_ERR, ACCEPT_ERR } type;
		server_pool_error(_type t) noexcept : type(t), classic_error() {}
	protected:
		virtual std::string descr () const;
	};
	
#ifndef XIF_NO_THREADS
	struct server_thread_data { void* _c; };
#endif
	
		/// Implementation methods
	namespace _socket_server {
		
			// Start listening state : create, bind, and put in listening state
		void _server_launch (socket_t sock, const sockaddr* addr, size_t addrlen, u_int listen_max, bool reuse);
		
			// Warper for accept() : return the new client's socket
		socket_t _server_accept (socket_t sock, sockaddr* addr, socklen_t* addrlen);
		
			// Create client thread
		pthread_t _server_cli_new_thread (void*(*thread_fnct)(server_thread_data*), void* new_client_ptr);
		
			// Some warpers for select()
		void _select_throw_stop (fd_t fd1, fd_t fd2, timeval timeout, bool ignsig); // Return on `fd1` activity, throw a `stop_exception` on `fd2` activity (fd2 priority)
		void _select_throw_stop (fd_t fd1, std::vector<fd_t>& fds, timeval timeout, bool ignsig); // Throw a `stop_exception` on any fd activity in `fds` (first fd in `fds` priority)
		uint _select (int maxsock, fd_set* set, timeval timeout); // Simple and transparent warper for select
		
			// Listening state exception
		extern const std::logic_error bad_state;
	}
	
	/***** Server-side (incoming) socket handler *****
	 *
	 * Bind to an address (ip:port, unix socket path...) and wait for clients
	 * Manage clients : a server client is a socket++ object of same type. 
	 *  This client object can hold an attached data of any type (<typename cli_data>). 
	 *  You can store, for example, client informations, like a client ID or a pointer to a
	 *  database structure or anything else.
	 * The client object (like any socket++ classes) and its data are destructed when no longer used, 
	 *  using reference counting. The server object can retain for you connected clients, so
	 *  you no longer need to manage your own connected clients list. Using this, the client is
	 *  released when connection end, or manually during connection.
	 * Clients can be put in pools, waiting for client activity (with optional file descriptor 
	 *  monitoring, like stdin). They can be put in autonomous threads. Or be processed 
	 *  by a callback funtion. Or simply, the server can be used to manage one client 
	 *  at a time in a loop, and close the connection after response.
	 */
	template <typename socket_base, typename cli_data_t>
	class socket_server : public socket_base {
	protected:
		
			/// Server base client
		class _base_client : public socket_base {
		public:
				// Addr info struct
			const typename socket_base::addr_info addr;
		protected:
			friend class socket_server;
				// Private constructors
			_base_client () = delete;
			_base_client (socket_t new_sock, typename socket_base::addr_info _addr) : socket_base(true, new_sock), addr(_addr) {}
			_base_client (const _base_client& other) : socket_base(other), addr(other.addr) {}
		};
		
			/// Client with data
		template <typename _data, typename dummy>
		class _client : public _base_client {
		private:
			REFCXX_REFCOUNTED(_server_client);
		protected:
			friend class socket_server;
				// User data
			_data* const _d;
				// Private constructors
			_client () = delete;
			_client (socket_t socket, typename socket_base::addr_info _addr) : _base_client(socket, _addr), REFCXX_CONSTRUCTOR(_server_client), _d(new _data) {}
			_client (const _client& other, cli_data_t* data) : _base_client(other), REFCXX_COPY_CONSTRUCTOR(_server_client, other), _d(new _data (*data)) {}
		public:
				// Copy constructor
			_client (const _client& other) : _base_client(other), REFCXX_COPY_CONSTRUCTOR(_server_client, other), _d(other._d) {}
#ifndef XIF_NO_THREADS
				// Constructor from server_thread_data
			_client (server_thread_data* d) : _client(*((_client*)d->_c)) { delete (_client*)d->_c; delete d; }
#endif
				// User data accessing
			const _data& operator-> () const { return *_d; }
			_data& operator-> () { return *_d; }
			const _data& data () const { return *_d; }
			_data& data () { return *_d; }
				// Destruct user data
			~_client () noexcept { REFCXX_DESTRUCT(_server_client) { delete _d; } }
		};
		
			/// Client without data
		template <typename dummy> 
		class _client<void, dummy> : public _base_client {
		protected:
			friend class socket_server;
				// Private constructors
			_client () = delete;
			_client (socket_t socket, typename socket_base::addr_info _addr) : _base_client(socket, _addr) {}
			_client (const _client& other, cli_data_t* data) : _base_client(other) {} // Data is ignored
		public:
				// Copy constructor
			_client (const _client& other) : _base_client(other) {}
#ifndef XIF_NO_THREADS
				// Constructor from server_thread_data
			_client (server_thread_data* d) : _client(*((_client*)d->_c)) { delete (_client*)d->_c; delete d; }
#endif
		};
		
			// Typedefs
	public:
		typedef _client<cli_data_t, void> client; // Server's client typedef
		
			// List of retained clients. Protected by a mutex.
	protected:
		std::list<client> retained_clients;
	public:
		typedef typename std::list<client>::iterator client_it;
		
			// Callbacks
	public:
		typedef std::function<pool_ret_t(client)> cli_callback_t;
		typedef std::function<pool_ret_t(fd_t)> fd_callback_t;
		
#ifndef XIF_NO_THREADS
		typedef void* (*cli_thread_routine_t) (server_thread_data*);
#endif
		
	protected:
		
			// Mutex for protecting methods that are using clients lists
#ifndef XIF_NO_THREADS
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		struct _mutex_lock { pthread_mutex_t* const _m; _mutex_lock (pthread_mutex_t& m) : _m(&m) { ::pthread_mutex_lock(_m); } ~_mutex_lock () { ::pthread_mutex_unlock(_m); } };
#endif
		
			// Server listen infos
		typename socket_base::addr_info listen_addr;
		bool listening;
		inline void chkl () { if (listening == false) throw _socket_server::bad_state; }
		
			// Timeout
		timeval pool_timeout;
		
			// Forbidden constructors
		socket_server () = delete;
		socket_server (const socket_server& other) = delete;
		socket_server (const socket_base& o) = delete;
		
	public:
		
			// Start (with the same address) and stop (release the port) listening for new clients
		void listening_start (uint listen_max, bool reuse) {
			if (listening == true) throw _socket_server::bad_state;
			auto _addr = listen_addr._getaddr();
			_addr.use(_addr_use_type_t::SERVER, *this);
			_socket_server::_server_launch(socket_base::fd, (const sockaddr*)&_addr.addr, _addr.len, listen_max, reuse);
			listening = true;
		}
		void listening_stop () {
			if (listening == false) throw _socket_server::bad_state;
			listen_addr._getaddr().unuse(_addr_use_type_t::SERVER, *this);
			socket_base::fd_close();
			listening = false;
		}
		
			// Constructor : set up the server
			// Take the addr struct for binding, the pending client queue for accepting (SOMAXCONN can be used if defined)
		socket_server (typename socket_base::addr_info addr, uint listen_max, bool reuse = false) : socket_base(), listen_addr(addr), listening(false), pool_timeout(TIMEOUT_INF) {
			this->listening_start(listen_max, reuse);
		}
			// Constructor, without starting listening
		socket_server (typename socket_base::addr_info addr) : socket_base(), listen_addr(addr), listening(false), pool_timeout(TIMEOUT_INF) {}
		
			// Destructor
		virtual ~socket_server () noexcept { /* no need to call listening_stop, these actions are automatic */ }
		
			// Retain client and return iterator.
		client_it retain_client (client& _client) { _mutex_lock _m(mutex); return retained_clients.insert(retained_clients.end(), _client); }
			// Release retained client. The client can be copied before to keep the connection opened.
		void release_client (client_it it)        { _mutex_lock _m(mutex); retained_clients.erase(it); }
		
			// Set pool-timeout, used as maximum wait timeout in pool methods. Null timeout disable it.
		void set_pool_timeout (timeval timeout)   { pool_timeout = timeout; }
		
			// Wait for new client, and optionally retain it
		client wait_new_client ();
		client_it wait_new_client_retained ()     { _mutex_lock _m(mutex); return retain_client(wait_new_client()); }
			// Pool-timeout aware version of wait_new_client. Ignore signals interrupts.
		client wait_new_client_timeout ()         { chkl(); _socket_server::_select_throw_stop(socket_base::fd, SOCKETXX_INVALID_HANDLE, pool_timeout, true); return wait_new_client(); }
			// Wait new client and interrupt if reveived changes on a pipe or any file descriptor. Pool-timeout aware. Can ignore signals interrupts.
		client wait_new_client_stoppable (fd_t fd_monitor, bool ignsig = false)        { chkl(); _socket_server::_select_throw_stop(socket_base::fd, fd_monitor, pool_timeout, ignsig); return wait_new_client(); }
		client wait_new_client_stoppable (std::vector<fd_t>& fds, bool ignsig = false) { chkl(); _socket_server::_select_throw_stop(socket_base::fd, fds, pool_timeout, ignsig); return wait_new_client(); }
		
#ifndef XIF_NO_THREADS
			// Create thread with client
		inline static pthread_t put_client_threaded (cli_thread_routine_t thread_fnct, const client& cli)     { return _socket_server::_server_cli_new_thread(thread_fnct, new client(cli)); }
		
			// Wait for new client and create thread for it
			// In the new thread, you can get client object with `server<...>::client cli(thread_data)` client constructor
			// If `cli_data_t` is void, attached_data is ignored
		client wait_new_client_threaded (cli_thread_routine_t thread_fnct, cli_data_t* attached_data = NULL)  { client c(wait_new_client(), attached_data); put_client_threaded(thread_fnct, c); return c; }
#endif
		
			// Pool all retained clients and wait for activity. The first awaked client in the list is returned.
		client_it wait_client_activity ()         { chkl(); _mutex_lock _m(mutex); return this->_wait_client_activity(); }
			// Fair version : avoid the first client in the list to always be handled before the others, by moving each awaked client to the end of the list. Iterators remain valid.
		client_it wait_client_activity_fair ()    { chkl(); _mutex_lock _m(mutex); client_it it = this->_wait_client_activity(); retained_clients.splice(retained_clients.end(),retained_clients,it); return it; }
		
			// Pool all retained clients and wait for activity in loop. If activity occurs, `cli_activity()` is called.
			// Quits if any callback returns `POOL_QUIT`. Timeout exceptions are thrown. Interruptions of syscalls are ignored.
			// The list of retained clients is scaned only once. To rescan it (eg. after client disconnection), `POOL_RESCAN` can be return from any callback.
			// Theses methods are not protected by the mutex and therefore only one pool should be used at a time for a socket_server
		void wait_activity_loop (cli_callback_t cli_activity_f)                                                                                                { _wait_activity_loop<false,false>(cli_activity_f,nullptr,{},nullptr); }
			// Additonally, wait for new clients. Must be in listening state. `new_client_f` is called with the new client accepted.
			// New clients are not retained automatically. If the new client is retained, `POOL_RESCAN` can be returned to rescan the list.
		void wait_activity_loop (cli_callback_t cli_activity_f, cli_callback_t new_client_f)                                                                   { _wait_activity_loop<true,false>(cli_activity_f,new_client_f,{},nullptr); }
			// Additonally, monitor activity on abritrary file descriptors (stdin, pipes, files…)
		void wait_activity_loop (cli_callback_t cli_activity_f, cli_callback_t new_client_f, fd_t fd_monitor,              fd_callback_t fd_activity_f)        { _wait_activity_loop<true,true>(cli_activity_f,new_client_f,{fd_monitor},fd_activity_f); }
		void wait_activity_loop (cli_callback_t cli_activity_f, cli_callback_t new_client_f, const std::vector<fd_t>& fds, fd_callback_t fd_activity_f)        { _wait_activity_loop<true,true>(cli_activity_f,new_client_f,fds,fd_activity_f); }
	
			// Iterate through the list of retained clients. `POOL_QUIT` can be returned from the callback to abort the iteration.
		void clients_foreach (cli_callback_t f)   { for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) { if (f(*it) == POOL_QUIT) break; }; }
		
			// Pool utility methods
	protected:
		template <bool newcli, bool monfds> void _wait_activity_loop (cli_callback_t, cli_callback_t, const std::vector<fd_t>&, fd_callback_t);
		int clients_fill_fdset (fd_set& s);
		client_it _wait_client_activity ();
	};
	
		///--- Implementation ---///
	
	template <typename socket_base, typename D>
	typename socket_server<socket_base,D>::client socket_server<socket_base,D>::wait_new_client () {
		chkl();
		typename socket_base::sockaddr_type addr;
		socklen_t len = sizeof(addr);
		socket_t new_fd = _socket_server::_server_accept(socket_base::fd, (sockaddr*)&addr, &len);
		typename socket_base::_addrt _addr({addr,len});
		client cli (new_fd, typename socket_base::addr_info(_addr));
		_addr.use(_addr_use_type_t::SERVER_CLI,cli);
		return cli;
	}
	
	template <typename socket_base, typename D>
	int socket_server<socket_base,D>::clients_fill_fdset (fd_set& s) {
		fd_t max = 0;
		FD_ZERO(&s);
		for (client& cli : retained_clients) {
			FD_SET(cli.fd, &s);
			if (cli.fd > max)
				max = cli.fd;
		}
		return max;
	}
	
	template <typename socket_base, typename D>
	typename socket_server<socket_base,D>::client_it socket_server<socket_base,D>::_wait_client_activity () {
		_mutex_lock _m(mutex);
		fd_set s;
		_socket_server::_select(this->clients_fill_fdset(s), &s, pool_timeout);
		for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) 
			if (FD_ISSET(it->fd, &s)) 
				return it;
	}
	
	template <typename socket_base, typename D> template <bool newcli, bool monfds>
	void socket_server<socket_base,D>::_wait_activity_loop (cli_callback_t client_activity_f, cli_callback_t new_client_f, const std::vector<fd_t>& fds, fd_callback_t fd_activity_f) {
		if (newcli) chkl();
		fd_set set;
		fd_t maxsock;
	_rescan:
		maxsock = this->clients_fill_fdset(set);
		if (newcli) {
			FD_SET(socket_base::fd, &set);
			if (socket_base::fd > maxsock) maxsock = socket_base::fd;
		}
		if (monfds)
			for (fd_t fd_monitor : fds) {
				FD_SET(fd_monitor, &set);
				if (fd_monitor > maxsock) maxsock = fd_monitor;
			}
		for (;;) {
			fd_set cpset = set;
			_socket_server::_select(maxsock, &cpset, pool_timeout);
			pool_ret_t r = POOL_CONTINUE;
			if (monfds)
				for (fd_t fd_monitor : fds) {
					if (FD_ISSET(fd_monitor, &cpset)) {
						r = fd_activity_f(fd_monitor);
						if (r != POOL_CONTINUE) goto _r_check;
					}
				}
			if (newcli)
				if (FD_ISSET(socket_base::fd, &cpset)) {
					client new_cli = wait_new_client();
					r = new_client_f(new_cli);
					if (r != POOL_CONTINUE) goto _r_check;
				}
			for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) {
				if (FD_ISSET(it->fd, &cpset)) {
					r = client_activity_f(*it);
					if (r != POOL_CONTINUE) goto _r_check;
				}
			}
		_r_check:
			if (r == POOL_RESCAN) goto _rescan;
			if (r == POOL_QUIT) return;
			
		}
	}
	
}}

#endif
