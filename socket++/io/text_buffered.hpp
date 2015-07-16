#ifndef SOCKET_XX_TEXT_BUFFERED_BASE_H
#define SOCKET_XX_TEXT_BUFFERED_BASE_H

	// BaseIO
#include <socket++/base_io.hpp>

	// General
#include <string>
#include <string.h>
#include <xifutils/traits.hpp>

namespace socketxx { namespace io {
	
	namespace _text_socket {
		
		typedef size_t (socketxx::base_fd::* i_fnct) (void *, size_t);
		typedef void (socketxx::base_fd::* o_fnct) (const void *, size_t);
		
		std::string read_line (socketxx::base_fd& sock, i_fnct readf, std::string& buffer, const char* sep);
		
		void write_txt (socketxx::base_fd& sock, o_fnct writef, const std::string& txt, const char* sep);
		
	}
	
		// Enabling text_socket<io_base> only for socketxx::base_fd derivatives
	template <typename io_base, typename = typename _enable_if_<std::is_base_of<socketxx::base_fd, io_base>::value>::type>
		class text_socket;
	
	/***** Text Buffered type I/O class *****
	 *
	 *  For use of plain old textual protocols like SMTP or HTTP.
	 *  Line-oriented.
	 *  You can define the line ending freely (\r\n or \n or everything else).
	 */
	
	template <typename io_base>
	class text_socket<io_base> : public io_base {
	protected:
		
		std::string buffer;
		const char* line_sep;
		
			// No copy
		text_socket (const text_socket& other) = delete;
			// Default constructor
		text_socket () : line_sep("\r\n"), buffer(std::string()) {}
			// Private relay constructor
		text_socket (bool autoclose_handle, socket_t handle) : io_base(autoclose_handle, handle) {}
		
	public:
		
			// Construct from an io_base object
		text_socket (const io_base& iob) : io_base(iob) {}
		
			// Set the line separator
		void set_line_sep (const char* sep)   { line_sep = sep; } // Must contain at least one char, max 256 chars
		
			// Read a line (without line ending)
		std::string i_line ()                 { return _text_socket::read_line(*this, this->_get_io_fncts().i, this->buffer, line_sep); }
		
			// Write line ending
		void o_line_ending ()                 { this->_o(line_sep, ::strlen(line_sep)); }
			// Write char string, as is
		void o_str (const std::string& str)   { this->_o(str.c_str(), str.length()); }
		void o_str (const char* str)          { this->_o(str, ::strlen(str)); }
			// Write a line (add the line ending)
		void o_line (const std::string& line) { o_str(line); o_line_ending(); } 
		void o_line (const char* line)        { o_str(line); o_line_ending(); }
			// Replace all '\n' by the line ending (and add final line ending if not present)
		void o_txt (const std::string& txt)   { _text_socket::write_txt(*this, this->_get_io_fncts().o, txt, line_sep); }
	};
	
}}

#endif
