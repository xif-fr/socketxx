#include <socket++/io/text_buffered.hpp>

	// OS headers
#include <unistd.h>

std::string socketxx::io::_text_socket::read_line (socketxx::base_fd& sock, i_fnct readf, std::string& buffer, const char* sep) {
	uint8_t sepsz = (uint8_t)::strlen(sep);
	for (;;) {
		if (buffer.length() >= sepsz)
			for (size_t i = 0; i < buffer.length()-sepsz+1; i++) {
				if (buffer[i] == sep[0]) {
					for (uint8_t c = 1; sep[c] != '\0'; c++) 
						if (sep[c] != buffer[i+c]) 
							goto _nextchar;
					std::string line = buffer.substr(0, i);
					buffer = buffer.substr(i+sepsz, std::string::npos);
					return line;
				}
			_nextchar:;
			}
		struct buff {
			size_t sz; char* b;
			buff () : b(NULL), sz((size_t)::getpagesize()) { b = new char[sz]; }
			~buff () { delete[] b; }
		} buf;
		size_t rs = (sock.*readf)(buf.b, buf.sz);
		buffer += std::string(buf.b, rs);
	}
}

void socketxx::io::_text_socket::write_txt(socketxx::base_fd &sock, o_fnct writef, const std::string &txt, const char* sep) {
	uint8_t sepsz = (uint8_t)::strlen(sep);
	size_t pos, i;
	for (pos = 0; pos < txt.length(); pos++) {
		for (i = pos; i < txt.length(); i++) {
			if (txt[i] == '\n') {
				if (i-pos > 0)
					(sock.*writef)(txt.c_str()+pos, i-pos);
				(sock.*writef)(sep, sepsz);
				pos = i;
				break;
			}
		}
	}
	if (pos < txt.length()) {
		(sock.*writef)(txt.c_str()+pos, txt.length()-pos);
		(sock.*writef)(sep, sepsz);
	}
}
