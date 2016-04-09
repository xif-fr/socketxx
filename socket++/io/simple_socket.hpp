#ifndef SOCKET_XX_SIMPLE_SOCKET_H
#define SOCKET_XX_SIMPLE_SOCKET_H

	// BaseIO
#include <socket++/base_io.hpp>

	// General
#include <inttypes.h>
#include <string>
#include <utility>
#include <xifutils/polyvar.hpp>
#include <type_traits>

	// OS headers
#include <unistd.h>

namespace socketxx { namespace io {
	
		// Private external functions
	namespace _simple_socket {
		
			// Swap bytes when foreign host don't have the same endianness
		void swapBytes (void* data_to_swap, size_t size);
		
			// Duplicate file descriptor for sending
		fd_t dup_fd (fd_t orig_fd);
		
			// File transfer functions
		typedef std::function< void (size_t done, size_t tot) > trsf_info_f;
		unsigned char* read_to_file (socketxx::base_fd& s, base_fd::_io_fncts::i_fnct i, base_fd::_io_fncts::o_fnct o, fd_t file_w, size_t sz, trsf_info_f); // Read from socket and write to file (return hash if enabled or NULL)
		auto_bdata write_from_file (socketxx::base_fd& s, base_fd::_io_fncts::i_fnct i, base_fd::_io_fncts::o_fnct o, fd_t file_r, size_t sz, trsf_info_f); // Read from file and write to socket
		bool same_hash (unsigned char* hash, auto_bdata s_hash); // Autodelete[] hashs
		fd_t create_temp_file (std::string& file_name); // file_name is only a prefix, not a template name
		size_t open_file_read (fd_t& filefd, const char* path); // Open a file and returns size
	}
	
		// Enabling simple_socket<io_base> only for socketxx::base_fd derivatives
	template <typename io_base, typename = typename std::enable_if<std::is_base_of<socketxx::base_fd, io_base>::value>::type>
		class simple_socket;
	
	/***** Simple_socket type I/O class *****
	 *
	 *  It is designed to be fast, stupid, and simple, free of transport problems.
	 *  It can transport different basic things :
	 *   - std::strings or char[], without caring of size problems
	 *   - integers of all sizes, without caring of endianness
	 *   - floats - warning, internal representation is assumed to be IEEE754 for non local networks and sizeof(double)=8
	 *   - a simple byte (char)
	 *   - a file, with MD5 checksum if socket++ is openssl-enabled on both side
	 *   - binary data with automatic alloc and dynamic size (max 4GiB) for receiver, with support of sending NULL
	 *   - binary buffers in a dumb manner, with sizes defined at both side, which shall coincide
	 *   - a socket itself, only on the *same* process, eg. between threads. The fd is dup(), so original sock can be destructed
	 *   - a xifutils polyvar, recursively if needed
	 *  The only thing you need to care about is the type of data and order : 
	 *   don't send a 64bits integer and try to receive a unsigned 16bits one.
	 *  Il will ofter results to an io_error, and it's very difficult to find the 
	 *   original problem (forget a send or receive query, or mismached integers sizes).
	 *  Stream-like I/O operators are available. 
	 */
	template <typename io_base>
	class simple_socket<io_base> : public io_base {
	protected:
		
			// Private relay constructors
		simple_socket (bool autoclose_handle, socket_t handle) : io_base(autoclose_handle, handle) {}
		simple_socket () : io_base() {}
			
	public:
		
			// Copy constructor
		simple_socket (const simple_socket<io_base>& other) : io_base(other) {}
			// Construct from an io_base object
		simple_socket (const io_base& iob) : io_base(iob) {}
		
	public:
		
			// Public read methods
		char i_char ()                                      { char c; io_base::_i_fixsize(&c, 1); return c; }
		bool i_bool ()                                      { bool b; io_base::_i_fixsize(&b, 1); return b; }
		std::string i_str ();
		template <typename int_t> int_t i_int ()            { int_t n; io_base::_i_fixsize(&n, sizeof(int_t)); return n; } // Endianness is already converted by sender
		double i_float ()                                   { int64_t t = this->i_int<int64_t>(); return *((double*)&t); } // Size of doubles must be 8 bytes and internal representation must be the same on both side
		size_t i_file (fd_t file_w, _simple_socket::trsf_info_f = NULL);                       // Return the file's size.
		std::string i_file (std::string file_prefix, _simple_socket::trsf_info_f = NULL);      // Create temporary file in tmp dir with template name. Return the file path. File is RW.
		void i_buf (void* buf, size_t len)                  { io_base::_i_fixsize(buf, len); } // Size is guaranteed to be the final read size
		void* i_bin (size_t& len)                           { len = i_int<uint32_t>(); if (!len) return NULL; void* p = new char[len]; io_base::_i_fixsize(p,len); return p; } // Need to be deleted[] if not NULL
		auto_bdata i_bin ()                                 { auto_bdata bd; bd.p = this->i_bin(bd.len); return bd; }      // Autodelete data with refcounting 
		socketxx::base_fd i_sock ()                         { fd_t fd = this->i_int<fd_t>(); return socketxx::base_fd(fd, true); }
		xif::polyvar i_var ();
			// Public write methods
		void o_char (char byte)                             { io_base::_o(&byte, 1); }
		void o_bool (bool b)                                { io_base::_o(&b, 1); }
		void o_str (const std::string& str);
		template <typename int_t> void o_int (int_t num)    { if (XIF_SOCKETXX_ENDIANNESS_SAME) { _simple_socket::swapBytes(&num, sizeof(int_t)); } io_base::_o(&num, sizeof(int_t)); }
		void o_float (double f)                             { this->o_int<int64_t>(*((int64_t*)&f)); }
		void o_file (fd_t file_r, size_t file_size, _simple_socket::trsf_info_f = NULL);
		void o_file (const char* path, _simple_socket::trsf_info_f = NULL);
		void o_buf (const void* buf, size_t len)            { io_base::_o(buf, len); }
		void o_bin (const void* p, size_t len)              { if (p == NULL) len = 0; this->o_int<uint32_t>((uint32_t)len); if (len != 0) io_base::_o(p, len); } // if len is 0, assuming NULL
		void o_sock (socketxx::base_fd& sock)               { sock.set_preserved(); fd_t new_fd = _simple_socket::dup_fd(sock.get_fd()); this->o_int<fd_t>(new_fd); } // dup the file descriptor, sock can be closed afetr
		void o_var (const xif::polyvar& var);
		
#ifndef XIF_SOCKETXX_NO_STREAM_OPERATORS
			// Public stream-like read
		simple_socket& operator>> (char& byte)              { byte = i_char(); return *this; }
		simple_socket& operator>> (bool& b)                 { b = i_bool(); return *this; }
		simple_socket& operator>> (std::string& str)        { str = i_str(); return *this; }
		template <typename int_t, typename std::enable_if<std::is_integral<int_t>::value and sizeof(int_t)!=1>::type* = nullptr>
		simple_socket& operator>> (int_t& integer)          { integer = i_int<int_t>(); return *this; }
			// Public stream-like write
		simple_socket& operator<< (char byte)               { o_char(byte); return *this; }
		simple_socket& operator<< (bool b)                  { o_bool(b); return *this; }
		simple_socket& operator<< (const std::string& str)  { o_str(str); return *this; }
		template <typename int_t, typename std::enable_if<std::is_integral<int_t>::value and sizeof(int_t)!=1>::type* = nullptr>
		simple_socket& operator<< (int_t integer)           { o_int<int_t>(integer); return *this; }
#endif
		
	};
	
	/*** Implementation ***/
	
		// String transfer functions
	template <typename io_base> 
	void simple_socket<io_base>::o_str (const std::string& str) {
		uint64_t str_len64 = (uint64_t)str.length();
		uint8_t str_len8 = (uint8_t)str_len64;
		if (str_len64 > 254) {
			str_len8 = 255;
			this->io_base::_o(&str_len8, 1);
			this->simple_socket::o_int<uint64_t>(str_len64);
		} else 
			this->io_base::_o(&str_len8, 1);
		if (str_len64 != 0)
			this->io_base::_o(str.c_str(), str_len64);
	}
	template <typename io_base> 
	std::string simple_socket<io_base>::i_str () {
		uint64_t str_len64;
		uint8_t str_len8;
		this->io_base::_i_fixsize(&str_len8, 1);
		if (str_len8 == 255) {
			str_len64 = this->simple_socket::i_int<uint64_t>();
		} else str_len64 = str_len8;
		if (str_len64 == 0)
			return std::string();
		else {
			char* str = new char[str_len64];
			try {
				this->io_base::_i_fixsize(str, str_len64);
			} catch (socketxx::error&) {
				delete[] str;
				throw;
			}
			std::string finalstr(str, str_len64);
			delete[] str;
			return finalstr;
		}
	}
	
		// Polyvar recursive functions
	template <typename io_base> 
	void simple_socket<io_base>::o_var (const xif::polyvar& p) {
		using namespace xif;
		this->o_char((char)p.type());
		switch (p.type()) {
			case polyvar::VOID:                               break;
			case polyvar::STR:   this->o_str(p.s());          break;
			case polyvar::FLOAT: this->o_float(p.f());        break;
			case polyvar::INT:   this->o_int<int64_t>(p.i()); break;
			case polyvar::CHAR:  this->o_char(p.c());         break;
			case polyvar::BOOL:  this->o_bool(p.b());         break;
			case polyvar::LIST: {
				const std::vector<xif::polyvar>& v = p.v();
				this->o_int<uint64_t>(v.size());
				for (size_t i = 0; i < v.size(); i++) 
					this->o_var(v[i]);
			} break;
			case polyvar::MAP: {
				const std::map<std::string,xif::polyvar>& m = p.m();
				this->o_int<uint64_t>(m.size());
				for (auto it = m.begin(); it != m.end(); it++) {
					this->o_str(it->first);
					this->o_var(it->second);
				}
			} break;
		}
	}
	template <typename io_base> 
	xif::polyvar simple_socket<io_base>::i_var () {
		using namespace xif;
		enum xif::polyvar::type t = (enum xif::polyvar::type)(this->i_char());
		switch (t) {
			case polyvar::VOID:  return xif::polyvar( );
			case polyvar::STR:   return xif::polyvar( this->i_str() );
			case polyvar::FLOAT: return xif::polyvar( this->i_float() );
			case polyvar::INT:   return xif::polyvar( this->i_int<int64_t>() );
			case polyvar::CHAR:  return xif::polyvar( this->i_char() );
			case polyvar::BOOL:  return xif::polyvar( this->i_bool() );
			case polyvar::LIST: {
				xif::polyvar p = std::vector<xif::polyvar>( this->i_int<uint64_t>() );
				for (size_t i = 0; i < p.v().size(); i++) 
					p.v()[i] = this->i_var();
				return p;
			}
			case polyvar::MAP: {
				xif::polyvar p = std::map<std::string,xif::polyvar>();
				size_t sz = this->i_int<uint64_t>();
				for (; sz != 0; sz--) 
					p.m().insert( std::pair<std::string,xif::polyvar>( this->i_str(), this->i_var() ));
				return p;
			}
		}
	}
	
		// File transfer functions
	template <typename io_base> 
	size_t simple_socket<io_base>::i_file (fd_t file_w, _simple_socket::trsf_info_f info_f) {
		size_t sz = this->i_int<uint64_t>();
		unsigned char* hash = _simple_socket::read_to_file(*this, this->_get_io_fncts().i, this->_get_io_fncts().o, file_w, sz, info_f);
		if (not _simple_socket::same_hash(hash, this->i_bin())) 
			throw socketxx::error("File transfer : MD5 checksums don't mach !");
		return sz;
	}
	template <typename io_base> 
	std::string simple_socket<io_base>::i_file (std::string file_name, _simple_socket::trsf_info_f info_f) {
		fd_t tempfd = INVALID_HANDLE;
		try {
			size_t sz = this->i_int<uint64_t>();
			tempfd = _simple_socket::create_temp_file(file_name);
			unsigned char* hash = _simple_socket::read_to_file(*this, this->_get_io_fncts().i, this->_get_io_fncts().o, tempfd, sz, info_f);
			if (not _simple_socket::same_hash(hash, this->i_bin())) 
				throw socketxx::error("File transfer : MD5 checksums don't mach !");
		} catch (...) {
			if (tempfd != INVALID_HANDLE) { ::close(tempfd); ::unlink(file_name.c_str()); }
			throw;
		}
		::close(tempfd);
		return file_name;
	}
	template <typename io_base> 
	void simple_socket<io_base>::o_file (fd_t file_r, size_t sz, _simple_socket::trsf_info_f info_f) {
		this->o_int<uint64_t>(sz);
		auto_bdata hash = _simple_socket::write_from_file(*this, this->_get_io_fncts().i, this->_get_io_fncts().o, file_r, sz, info_f);
		this->o_bin(hash.p, hash.len);
	}
	template <typename io_base> 
	void simple_socket<io_base>::o_file (const char* path, _simple_socket::trsf_info_f info_f) {
		fd_t filefd = INVALID_HANDLE;
		size_t sz = _simple_socket::open_file_read(filefd, path);
		try {
			this->o_file(filefd, sz);
		} catch (...) {
			::close(filefd); throw;
		}
		::close(filefd);
	}
	
}}
	
#endif
