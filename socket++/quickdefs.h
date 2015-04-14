namespace socketxx {
	
	#if defined(SOCKET_XX_CLIENT_H) && defined(SOCKET_XX_SIMPLE_SOCKET_H) && !defined(SOCKETXX_SIMPLE_SOCKET_CLIENT)
		template <typename socket_base> 
			using simple_socket_client = end::socket_client<io::simple_socket<socket_base>>;
		#define SOCKETXX_SIMPLE_SOCKET_CLIENT
	#endif
	#if defined(SOCKET_XX_HANDLER_SOCKET_SERVER_H) && defined(SOCKET_XX_SIMPLE_SOCKET_H) && !defined(SOCKETXX_SIMPLE_SOCKET_SERVER)
		template <typename socket_base, typename cli_data_t> 
			using simple_socket_server = end::socket_server<io::simple_socket<socket_base>,cli_data_t>;
		#define SOCKETXX_SIMPLE_SOCKET_SERVER
	#endif
	
}