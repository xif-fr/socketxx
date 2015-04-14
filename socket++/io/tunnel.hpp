#ifndef SOCKET_XX_TUNNEL_H
#define SOCKET_XX_TUNNEL_H

#include <socket++/base_io.hpp>

namespace socketxx { namespace io {
	
		// Private external warper
	namespace _tunnel {
		
		typedef size_t (socketxx::base_fd::* i_fnct) (void *, size_t);
		typedef void (socketxx::base_fd::* o_fnct) (const void *, size_t);
		
			// Simple select/read&write/buffer tunneling, but very inefficient
		void do_copy_tunneling (socketxx::base_fd& s1, i_fnct i1, o_fnct o1, socketxx::base_fd& s2, i_fnct i2, o_fnct o2, void(*f)(bool,void**, size_t*, size_t), timeval timeout);
		
	}
	
	template <typename io_base, typename io_base_other>
	class tunnel : public io_base {
	protected:
		
			// No copy
		tunnel (const tunnel& other) = delete;
		tunnel () = delete;
		
	public:
		
			// Construct from an io_base object
		tunnel (const io_base& iob) : io_base(iob) {}
		
	public:
		
			// Start tunneling with another socket. Blocks until disconnection of one side
		void start_tunneling (socketxx::base_fd& other) {
			socketxx::base_fd::_io_fncts other_io_fncts = (((io_base_other*)&other)->*&tunnel::_get_io_fncts)();
			_tunnel::do_copy_tunneling(*this, this->_get_io_fncts().i, this->_get_io_fncts().o,
			                           other, other_io_fncts.i, other_io_fncts.o,
												NULL, this->get_read_timeout());
		}
			// Tunneling with intercepting callback. If returned len if bigger than buf_sz, the internal buffer is replaced by yours (allocated by new char[len])
		typedef void (*intercept_fnct_t) (bool this_to_other, void** buf, size_t* len, size_t buf_sz);
		void start_tunneling (socketxx::base_fd& other, intercept_fnct_t callback) {
			socketxx::base_fd::_io_fncts other_io_fncts = (((io_base_other*)&other)->*&tunnel::_get_io_fncts)();
			_tunnel::do_copy_tunneling(*this, this->_get_io_fncts().i, this->_get_io_fncts().o,
			                           other, other_io_fncts.i, other_io_fncts.o,
												callback, this->get_read_timeout());
		}
		
	};
	
}}

#endif
