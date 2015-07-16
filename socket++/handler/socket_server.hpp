#ifndef SOCKET_XX_HANDLER_SOCKET_SERVER_H
#define SOCKET_XX_HANDLER_SOCKET_SERVER_H

	// BaseIO
#include <socket++/base_io.hpp>

	// General headers
#include <list>
#include <vector>
#ifndef XIF_NO_CXX11_STL
	#include <functional>
#elif !defined(XIF_NO_STD_FUNCTION)
	#define XIF_NO_STD_FUNCTION
#endif

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
	private:
		std::string _str () const noexcept;
	public:
		enum _type { BIND, LISTEN } type;
		server_launch_error(_type t) throw() : type(t), socketxx::classic_error(socket_errno) { socket_errno_reset; this->descr = this->_str(); }
	};
		// select() error or timeout
	class server_select_error : public socketxx::classic_error {
	public:
		server_select_error() noexcept : classic_error("Server select() error during client activity waiting",socket_errno) { socket_errno_reset; }
	};
		// accept() error
	class server_accept_error : public socketxx::classic_error {
	public:
		server_accept_error() noexcept : classic_error("Server accept() error",socket_errno) { socket_errno_reset; }
	};
		// bad listening state
	class server_listening_state : public socketxx::error {
	public:
		server_listening_state() noexcept : socketxx::error("bad server listening state") { };
	};
	
#ifndef XIF_NO_THREADS
	struct server_thread_data { void* _c; };
#endif
	
		/// Private functions
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
		
			// Retained clients list for pooling. Protected by a mutex.
	protected:
		std::list<client> retained_clients;
		typedef typename std::list<client>::iterator client_it;
		
			// Callbacks
	public:
#ifndef XIF_NO_STD_FUNCTION
		typedef std::function<pool_ret_t(client)> cli_callback_t;
		typedef std::function<pool_ret_t(fd_t)> fd_callback_t;
#else // For those who haven't a complete C++11 library
		typedef pool_ret_t (*cli_callback_t) (client);
		typedef pool_ret_t (*fd_callback_t) (fd_t);
#endif
		
#ifndef XIF_NO_THREADS
		typedef void*(*cli_thread_routine_t)(server_thread_data*);
#endif
		
	protected:
		
			// Some utility methods
		int clients_fill_fdset (fd_set& s) { fd_t max = 0; FD_ZERO(&s); for (client& cli : retained_clients) { FD_SET(cli.fd, &s); if (cli.fd > max) max = cli.fd; } return max; }
		client_it _wait_client_activity ();
		
			// Mutex for protecting methods that are using clients lists
#ifndef XIF_NO_THREADS
		pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
		struct _mutex_lock { pthread_mutex_t* const _m; _mutex_lock (pthread_mutex_t& m) : _m(&m) { ::pthread_mutex_lock(_m); } ~_mutex_lock () { ::pthread_mutex_unlock(_m); } };
#endif
		
			// Server listen infos
		typename socket_base::addr_info listen_addr;
		bool listening;
		inline void chkl () { if (listening == false) throw server_listening_state(); }
		
			// Timeout
		timeval pool_timeout;
		
			// Forbidden constructors
		socket_server () = delete;
		socket_server (const socket_server& other) = delete;
		socket_server (const socket_base& o) = delete;
		
	public:
		
			// Start (with the same address) and stop (release the port) listening for new clients
		void listening_start (uint listen_max, bool reuse) {
			if (listening == true) throw server_listening_state();
			auto _addr = listen_addr._getaddr();
			_addr.use(_addr_use_type_t::SERVER, *this);
			_socket_server::_server_launch(socket_base::fd, (const sockaddr*)&_addr.addr, _addr.len, listen_max, reuse);
			listening = true;
		}
		void listening_stop () {
			if (listening == false) throw server_listening_state();
			listen_addr._getaddr().unuse(_addr_use_type_t::SERVER, *this);
			socket_base::fd_close();
			listening = false;
		}
		
			// Constructor : set up the server
			// Take the addr struct for binding, the pending client queue for accepting (you can use SOMAXCONN if defined)
		socket_server (typename socket_base::addr_info addr, uint listen_max, bool reuse = false) : socket_base(), listen_addr(addr), listening(false), pool_timeout(NULL_TIMEVAL) {
			this->listening_start(listen_max, reuse);
		}
			// Constructor, without starting listening
		socket_server (typename socket_base::addr_info addr) : socket_base(), listen_addr(addr), listening(false), pool_timeout(NULL_TIMEVAL) {}
		
			// Destructor
		virtual ~socket_server () noexcept { /* no need to call listening_stop, these actions are automatic */ }
		
			// Retain client and return iterator.
		client_it retain_client (client& _client) { ::pthread_mutex_lock(&mutex); return _client._it = retained_clients.insert(retained_clients.end(), _client); ::pthread_mutex_unlock(&mutex); }
			// Release retained client. You can get client before releasing if you don't want to close the connection.
		void release_client (client_it it)        { ::pthread_mutex_lock(&mutex); retained_clients->_it = client_it(); retained_clients.erase(it); ::pthread_mutex_unlock(&mutex); }
		
			// Wait for new client an optionally retain it
		client wait_new_client ()                 { chkl(); typename socket_base::sockaddr_type addr; socklen_t len = sizeof(addr); socket_t new_fd = _socket_server::_server_accept(socket_base::fd, (sockaddr*)&addr, &len); typename socket_base::_addrt _addr({addr,len}); client cli (new_fd, typename socket_base::addr_info(_addr)); _addr.use(_addr_use_type_t::SERVER_CLI,cli); return cli; }
		client_it wait_new_client_retained ()     { _mutex_lock _m(mutex); return retain_client(wait_new_client()); }
			// Pool-timeout aware version of wait_new_client. Ignore signals interrupts.
		client wait_new_client_timeout ()         { chkl(); _socket_server::_select_throw_stop(socket_base::fd, INVALID_SOCKET, pool_timeout, true); return wait_new_client(); };
			// Wait new client and interrupt if reveived changes on a pipe or any file descriptor. Pool-timeout aware. Can ignore signals interrupts.
		client wait_new_client_stoppable (fd_t fd_monitor, bool ignsig = false)         _SOCKETXX_UNIX_IMPL({ chkl(); _socket_server::_select_throw_stop(socket_base::fd, fd_monitor, pool_timeout, ignsig); return wait_new_client(); });
		client wait_new_client_stoppable (std::vector<fd_t>& fds, bool ignsig = false)  _SOCKETXX_UNIX_IMPL({ chkl(); _socket_server::_select_throw_stop(socket_base::fd, fds, pool_timeout, ignsig); return wait_new_client(); });
		
#ifndef XIF_NO_THREADS
			// Create thread with client
		inline static pthread_t put_client_threaded (cli_thread_routine_t thread_fnct, const client& cli)     { return _socket_server::_server_cli_new_thread(thread_fnct, new client(cli)); }
		
			// Wait for new client and create thread for it
			// In the new thread, you can get client object with `server<...>::client cli(thread_data)` client constructor
			// If `cli_data_t` is void, attached_data is ignored
		client wait_new_client_threaded (cli_thread_routine_t thread_fnct, cli_data_t* attached_data = NULL)  { client c(wait_new_client(), attached_data); put_client_threaded(thread_fnct, c); return c; }
#endif
		
			// Pool all retained clients and wait for activity. Returns iterator of first awaked client in the list.
		client_it wait_client_activity ()         { chkl(); _mutex_lock _m(mutex); return this->_wait_client_activity(); }
			// Fair version : avoids the firsts clients in the list to always be handled before the others, by moving them to the end of the list. Iterators remain valid.
		client_it wait_client_activity_fair ()    { chkl(); _mutex_lock _m(mutex); client_it it = this->_wait_client_activity(); retained_clients.splice(retained_clients.end(),retained_clients,it); return it; }
		
			// Pool all retained clients and wait for activity in loop. If activity occurs, calls back `cli_activity()` function or lambda.
			// The loop is quitted if a callback ruturns `POOL_QUIT`. A timeout exception can occurs. Interruptions of syscalls are ignored.
			// /!\ For performance issues, the retained-clients list is scaned only once. If the list is updated (client disconnect for example), you must return `POOL_RESCAN` in the callback.
			// Theses methods are NOT protected by the mutex and therefore only one pool should be used at a time for a socket_server
		void wait_activity_loop (cli_callback_t cli_activity);
			// Waits also for new clients and calls back `new_client` with the freshly accepted but not retained client.
			// After each `new_client()` callback, the retained-client list is re-scanned, so any change in this list will be seen.
		void wait_activity_loop (cli_callback_t cli_activity, cli_callback_t new_client);
			// With `fd_activity` callback defined, you can also monitor inputs on any file descriptors (stdin, pipes, files...)
		void wait_activity_loop (cli_callback_t cli_activity, cli_callback_t new_client, const std::vector<fd_t>& fds, fd_callback_t fd_activity) _SOCKETXX_WIN_DELETE;
			// Specialized version : This one monitors only one additional file descriptor
		void wait_activity_loop (cli_callback_t cli_activity, cli_callback_t new_client, fd_t fd_monitor, fd_callback_t fd_activity) _SOCKETXX_WIN_DELETE;
		
			// Set pool-timeout, used as maximum wait timeout in pool methods. Null timeout disable it.
		void set_pool_timeout (timeval timeout) { pool_timeout = timeout; }
		
	};
		
		///--- Implementations ---///
	
	template <typename socket_base, typename D>
	typename socket_server<socket_base,D>::client_it socket_server<socket_base,D>::_wait_client_activity () {
		_mutex_lock _m(mutex);
		fd_set s;
		_socket_server::_select(this->clients_fill_fdset(s), &s, pool_timeout);
		for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) 
			if (FD_ISSET(it->fd, &s)) 
				return it;
	}
		
	template <typename socket_base, typename D>
	void socket_server<socket_base,D>::wait_activity_loop (cli_callback_t client_activity) {
		chkl();
		fd_set set;
		fd_t maxsock;
	_rescan:
		maxsock = this->clients_fill_fdset(set);
		for (;;) {
			fd_set cpset = set;
			_socket_server::_select(maxsock, &cpset, pool_timeout);
			for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) {
				if (FD_ISSET(it->fd, &cpset)) {
					pool_ret_t r = client_activity(*it);
					if (r != POOL_CONTINUE) { if (r == POOL_RESCAN) goto _rescan; if (r == POOL_QUIT) return; }
				}
			}
		}
	}
	
	template <typename socket_base, typename D>
	void socket_server<socket_base,D>::wait_activity_loop (cli_callback_t client_activity, cli_callback_t new_client) {
		chkl();
		fd_set set;
		fd_t maxsock;
	_rescan:
		maxsock = this->clients_fill_fdset(set);
		FD_SET(socket_base::fd, &set);
		if (maxsock < socket_base::fd) maxsock = socket_base::fd;
		for (;;) {
			fd_set cpset = set;
			_socket_server::_select(maxsock, &cpset, pool_timeout);
			if (FD_ISSET(socket_base::fd, &cpset)) { // New client !
				client new_cli = wait_new_client();
				if (new_client(new_cli) == POOL_QUIT) return;
				goto _rescan;
			}
			for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) {
				if (FD_ISSET(it->fd, &cpset)) {
					pool_ret_t r = client_activity(*it);
					if (r != POOL_CONTINUE) { if (r == POOL_RESCAN) goto _rescan; if (r == POOL_QUIT) return; }
				}
			}
		}
	}
	
#ifndef _WIN32
	
	template <typename socket_base, typename D>
	void socket_server<socket_base,D>::wait_activity_loop (cli_callback_t client_activity, cli_callback_t new_client, const std::vector<fd_t>& fds, fd_callback_t fd_activity) {
		chkl();
		fd_set set;
		fd_t maxsock;
	_rescan:
		maxsock = this->clients_fill_fdset(set);
		FD_SET(socket_base::fd, &set);
		if (socket_base::fd > maxsock) maxsock = socket_base::fd;
		for (fd_t fd_monitor : fds) {
			FD_SET(fd_monitor, &set);
			if (fd_monitor > maxsock) maxsock = fd_monitor;
		}
		for (;;) {
			fd_set cpset = set;
			_socket_server::_select(maxsock, &cpset, pool_timeout);
			if (FD_ISSET(socket_base::fd, &cpset)) { // New client !
				client new_cli = wait_new_client();
				if (new_client(new_cli) == POOL_QUIT) return;
				goto _rescan;
			}
			for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) {
				if (FD_ISSET(it->fd, &cpset)) {
					pool_ret_t r = client_activity(*it);
					if (r != POOL_CONTINUE) { if (r == POOL_RESCAN) goto _rescan; if (r == POOL_QUIT) return; }
				}
			}
			for (fd_t fd_monitor : fds) { // Check for file descriptor activity
				if (FD_ISSET(fd_monitor, &cpset)) {
					pool_ret_t r = fd_activity(fd_monitor);
					if (r != POOL_CONTINUE) { if (r == POOL_RESCAN) goto _rescan; if (r == POOL_QUIT) return; }
				}
			}
		}
	}
	
	template <typename socket_base, typename D>
	void socket_server<socket_base,D>::wait_activity_loop (cli_callback_t client_activity, cli_callback_t new_client, fd_t fd_monitor, fd_callback_t fd_activity) {
		chkl();
		fd_set set;
		fd_t maxsock;
	_rescan:
		maxsock = this->clients_fill_fdset(set);
		FD_SET(socket_base::fd, &set);
		if (socket_base::fd > maxsock) maxsock = socket_base::fd;
		FD_SET(fd_monitor, &set);
		if (fd_monitor > maxsock) maxsock = fd_monitor;
		for (;;) {
			fd_set cpset = set;
			_socket_server::_select(maxsock, &cpset, pool_timeout);
			if (FD_ISSET(fd_monitor, &cpset)) { // File descriptor activity !
				pool_ret_t r = fd_activity(fd_monitor);
				if (r != POOL_CONTINUE) { if (r == POOL_RESCAN) goto _rescan; if (r == POOL_QUIT) return; }
			}
			if (FD_ISSET(socket_base::fd, &cpset)) { // New client !
				client new_cli = wait_new_client();
				if (new_client(new_cli) == POOL_QUIT) return;
				goto _rescan;
			}
			for (client_it it = retained_clients.begin(); it != retained_clients.end(); ++it) {
				if (FD_ISSET(it->fd, &cpset)) {
					pool_ret_t r = client_activity(*it);
					if (r != POOL_CONTINUE) { if (r == POOL_RESCAN) goto _rescan; if (r == POOL_QUIT) return; }
				}
			}
		}
	}
	
#endif /*_WIN32*/

}}

#endif
