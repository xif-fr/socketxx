#ifndef SOCKET_XX_BASE_DEFS_H
#define SOCKET_XX_BASE_DEFS_H

	// Config header
#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

	// OS headers and defs
#include <errno.h>
#define errno_reset errno=0
#include <sys/types.h>
typedef int fd_t;
typedef fd_t socket_t;
#define SOCKETXX_INVALID_HANDLE -1

	// Exceptions
#include <xifutils/dexcept.hpp>

	// Endianness
// Define endianness (from Autoconf WORDS_BIGENDIAN or system defs)
#define XIF_SOCKETXX_BIG_ENDIAN 1
#define XIF_SOCKETXX_LITTLE_ENDIAN 0
#ifdef CONFIG_H_INCLUDED
	#ifdef WORDS_BIGENDIAN
		#define XIF_SOCKETXX_ENDIANNESS XIF_SOCKETXX_BIG_ENDIAN
	#else
		#define XIF_SOCKETXX_ENDIANNESS XIF_SOCKETXX_LITTLE_ENDIAN
	#endif
#else
	#ifndef XIF_SOCKETXX_ENDIANNESS
		#warning TO DO : Endianness without Autoconf's WORDS_BIGENDIAN
	#endif
#endif
// Endianness priority. /!\WARNING/!\ If socket++ is build with big-endian priority, integer routines
//  will not be compatible with little-endian socket++ builds. Change this only for an isolated network system.
#define XIF_SOCKETXX_ENDIANNESS_PRIORITY XIF_SOCKETXX_LITTLE_ENDIAN  // Little endian priority (x86 still rules the world)
#define XIF_SOCKETXX_ENDIANNESS_SAME (XIF_SOCKETXX_ENDIANNESS_PRIORITY == XIF_SOCKETXX_ENDIANNESS)

	// Reference counting
#include <xifutils/refcount++.hpp>

	// Time
#include <sys/time.h>
// Timeval comparing
inline bool operator== (timeval first, timeval second) { return (first.tv_sec == second.tv_sec) && (first.tv_usec == second.tv_usec); }
inline bool operator!= (timeval first, timeval second) { return !(first == second); }
// Timeval 0
#define NULL_TIMEVAL timeval({0,0})

namespace socketxx {

	/** ------ Helpers ------ **/
	
		// Autodelete binary data buffer
	struct auto_bdata : public refcountxx_base {
		void* p;
		size_t len;
		auto_bdata () : p(NULL), len(0) {}
		auto_bdata (void* p) : p(p) {}
		auto_bdata (const auto_bdata& o) : refcountxx_base(o), p(o.p), len(o.len) {}
		~auto_bdata() { if (this->can_destruct() and p != NULL) { delete[] (unsigned char*)p; } }
	};
	
		// Simple flag helper to add or clear flags
	class flags {
	protected:
		virtual int get () const = 0;
		virtual void set (int) = 0;
		virtual ~flags() {};
		#define _SOCKETXX_FLAGS_COMMON(class) protected: virtual int get () const; virtual void set (int); public: virtual ~class();
	public:
		flags& operator+= (int flag) { (*this) |= flag; return *this; }
		flags& operator-= (int flag) { (*this) &= ~flag; return *this; }
		flags& operator&= (int flags) { int f = this->get(); f &= flags; this->set(f); return *this; }
		flags& operator|= (int flags) { int f = this->get(); f |= flags; this->set(f); return *this; }
		flags& operator= (int flags) { this->set(flags); return *this; }
		bool operator& (int flag) const { int f = this->get(); return f & flag; }
	};
	
	enum rw_t { READ, WRITE };
	
	/** ------ Exceptions ------ **/
	
		// Generic socket++ event exception
	class event : public std::exception {
	public:
		event () noexcept : std::exception() {}
		virtual const char* what() const noexcept = 0;
		virtual ~event () noexcept {};
	};
	
		// Generic socket++ error exception
	class error : public xif::dexcept {
	protected:
		virtual std::string descr () const { return std::string(); }
		error () noexcept : xif::dexcept() {}
	public:
		explicit error (const char* what) : xif::dexcept(what) {}
		virtual ~error() noexcept {}
	};
	
}

#endif