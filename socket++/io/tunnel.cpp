#include <socket++/io/tunnel.hpp>

	// OS headers
#include <unistd.h>
#include <sys/select.h>

namespace socketxx { namespace io {
	
		// Simple select/read&write/buffer tunneling, but very inefficient due to the two userspace copies
	void _tunnel::do_copy_tunneling (socketxx::base_fd& s1, base_fd::_io_fncts::i_fnct i1, base_fd::_io_fncts::o_fnct o1, socketxx::base_fd& s2, base_fd::_io_fncts::i_fnct i2, base_fd::_io_fncts::o_fnct o2, void(*f)(bool,void**, size_t*, size_t), timeval timeout) {
		struct buffer {
			size_t sz;
			void* b;
			buffer () : b(NULL), sz((size_t)::getpagesize()) { b = new char[sz]; }
			void replace (void* new_p, size_t new_len) { delete [] (char*)b; b = new_p; sz = new_len; }
			~buffer () { delete [] (char*)b; }
		} buf;
		int r;
		fd_t fd1 = s1.base_fd::get_fd();
		fd_t fd2 = s2.base_fd::get_fd();
		fd_set select_set;
		fd_t maxfd = (fd1 > fd2) ? fd1+1 : fd2+1;
		for (;;) {
			FD_ZERO(&select_set);
			FD_SET(fd1, &select_set);
			FD_SET(fd2, &select_set);
			timeval tm = timeout;
			r = ::select(maxfd, &select_set, NULL, NULL, (timeout==TIMEOUT_INF)?(timeval*)NULL:&tm);
			if (r == -1) {
				if (errno == EINTR) continue;
				throw socketxx::other_error("select() error while tunneling");
			}
			if (r == 0) throw socketxx::timeout_event();
			try {
				if (FD_ISSET(fd1, &select_set)) {
					size_t len = (s1.*i1)(buf.b, buf.sz);
					if (f != NULL) {
						void* buf_p = buf.b;
						(*f)(true, &buf_p, &len, buf.sz);
						if (len > buf.sz) buf.replace(buf_p, len);
					}
					(s2.*o2)(buf.b, len);
				}
				if (FD_ISSET(fd2, &select_set)) {
					size_t len = (s2.*i2)(buf.b, buf.sz);
					if (f != NULL) {
						void* buf_p = buf.b;
						(*f)(false, &buf_p, &len, buf.sz);
						if (len > buf.sz) buf.replace(buf_p, len);
					}
					(s1.*o1)(buf.b, len);
				}
			} catch (socketxx::io_error& io_err) {
				if (io_err.is_connection_closed()) return;
				else throw;
			}
		}
	}

}}
