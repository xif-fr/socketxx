#include <socket++/io/simple_socket.hpp>

	// OS headers
#include <sys/mman.h>
#ifdef XIF_USE_SSL
	#include <openssl/md5.h>
#endif
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

	/// Swap bytes when foreign host have not the same endianness
void socketxx::io::_simple_socket::swapBytes(void* data_to_swap, size_t size) { // use bswap instructions
	uint8_t* data = (uint8_t*)data_to_swap;
	uint8_t tmp;
	for (size_t i = 0; i < size/2; ++i) {
		tmp = data[i];
		data[i] = data[size-1-i];
		data[size-1-i] = tmp;
	}
}

	/// Duplicate file descriptor for sending sock
fd_t socketxx::io::_simple_socket::dup_fd (fd_t orig_fd) {
	fd_t r_fd;
	r_fd = ::dup(orig_fd);
	if (r_fd == -1)
		throw socketxx::other_error("can't duplicate file descriptor for sending");
	return r_fd;
}

	/// Create temporary file
fd_t socketxx::io::_simple_socket::create_temp_file (std::string& file_name) {
	std::string tempfile_name;
	uint16_t i;
	do {
		long rand = (long)::rand()*(long)::clock();
		if (rand <= 0) continue;
		char* randstr = ::l64a(rand);
		for (uint8_t i = 0; i < 6; i++) if (not isalnum(randstr[i])) randstr[i] = '+';
		tempfile_name = file_name+"-"+randstr;
		fd_t filefd = ::open(tempfile_name.c_str(), O_CREAT|O_EXCL|O_WRONLY|O_NOFOLLOW, 0600);
		if (filefd == -1) {
			if (errno == EEXIST) continue;
			else throw socketxx::other_error(std::string("failed to create temp file '")+tempfile_name+"'");
		}
		file_name = tempfile_name;
		return filefd;
	} while (++i < UINT16_MAX);
	throw socketxx::error("failed to create temp file : too many attempts");
}

size_t socketxx::io::_simple_socket::open_file_read (fd_t& filefd, const char* path) {
	struct stat fst;
	int r = ::stat(path, &fst);
	if (r == -1)
		throw socketxx::other_error("file send : stat() failed");
	if ((fst.st_mode & S_IFMT) != S_IFREG)
		throw socketxx::error("file send : not a regular file");
	size_t sz = (size_t)fst.st_size;
	filefd = ::open(path, O_RDONLY);
	if (filefd == -1)
		throw socketxx::other_error("file send : open() failed");
	return sz;
}

	/// Read from socket and write to file, with classical copy to userspace
unsigned char* socketxx::io::_simple_socket::read_to_file (socketxx::base_fd& s, base_fd::_io_fncts::i_fnct i, base_fd::_io_fncts::o_fnct o, fd_t file_w, size_t sz, _simple_socket::trsf_info_f info_f) {
	if (sz == 0) return NULL;
	int r;
#ifdef XIF_USE_SSL
	MD5_CTX hashctx; MD5_Init(&hashctx);
#endif
	size_t chunksz = (size_t)::getpagesize() * 16;
	struct buffer {
		void* b;
		buffer (size_t chunksz) : b(NULL) { b = new char[chunksz]; }
		~buffer () { delete [] (char*)b; }
	} buf(chunksz);
	::lseek(file_w, 0, SEEK_SET);
	size_t bytes_rest = sz;
	while (bytes_rest != 0) {
		if (bytes_rest < chunksz) 
			chunksz = bytes_rest;
		errno_reset;
		ssize_t rs;
		size_t recsz;
		recsz = (s.*i)(buf.b, chunksz);
		bytes_rest -= recsz;
	#ifdef XIF_USE_SSL
		r = MD5_Update(&hashctx, buf.b, recsz);
		if (r != 1)
			throw socketxx::error("file transfer : MD5_Update() failed");
	#endif
		if (info_f)
			info_f (sz-bytes_rest, sz);
		size_t file_rest = (size_t)recsz;
		rs = ::write(file_w, buf.b, file_rest);
	_err:
		if (rs < 1) 
			throw socketxx::other_error("read_to_file : failed to write to file");
		if ((size_t)rs != file_rest) {
			uint8_t* b = (uint8_t*)buf.b + rs;
			file_rest -= (size_t)rs;
			while (file_rest != 0) {
				file_rest -= (size_t)rs;
				b += rs;
				rs = ::write(file_w, b, file_rest);
				if (rs < 1) 
					goto _err;
			}
		}
	}
#ifdef XIF_USE_SSL
	unsigned char* hash = new unsigned char[MD5_DIGEST_LENGTH];
	r = MD5_Final(hash, &hashctx);
	if (r != 1)
		throw socketxx::error("file transfer : MD5_Final() failed ");
	return hash;
#else
	return NULL;
#endif
}

	/// Write from file to socket using mmap for zero-copy behavior
socketxx::auto_bdata socketxx::io::_simple_socket::write_from_file (socketxx::base_fd& s, base_fd::_io_fncts::i_fnct i, base_fd::_io_fncts::o_fnct o, fd_t file_r, size_t sz, _simple_socket::trsf_info_f info_f) {
	if (sz == 0) return NULL;
	int r;
#ifdef XIF_USE_SSL
	MD5_CTX hashctx; MD5_Init(&hashctx);
#endif
	size_t bytes_done = 0;
	size_t chunksz = (size_t)::getpagesize() * 16;
	size_t chunk_rest = sz / chunksz;
	for (off_t off_f = 0;; off_f += chunksz) {
		if (chunk_rest == 0) {
			if (sz%chunksz != 0) chunksz = sz%chunksz;
			else break; }
		void* mapchunk = ::mmap(NULL, chunksz, PROT_READ, MAP_SHARED|MAP_FILE, file_r, off_f);
		if (mapchunk == MAP_FAILED) 
			throw socketxx::other_error("file send : mmap() failed");
		(s.*o)(mapchunk, chunksz);
	#ifdef XIF_USE_SSL
		r = MD5_Update(&hashctx, mapchunk, chunksz);
		if (r != 1)
			throw socketxx::error("file send : MD5_Update() failed");
	#endif
		::munmap(mapchunk, chunksz);
		if (info_f) {
			bytes_done += chunksz;
			info_f (bytes_done, sz);
		}
		if (chunk_rest == 0) break;
		chunk_rest--;
	}
#ifdef XIF_USE_SSL
	auto_bdata hash = new unsigned char[MD5_DIGEST_LENGTH];
	hash.len = MD5_DIGEST_LENGTH;
	r = MD5_Final((unsigned char*)hash.p, &hashctx);
	if (r != 1)
		throw socketxx::error("file transfer : MD5_Final() failed");
	return hash;
#else
	return NULL;
#endif
}

	// Check checksums hashs. Autodelete[] hashs if not NULL.
bool socketxx::io::_simple_socket::same_hash (unsigned char* hash, auto_bdata s_hash) {
#ifdef XIF_USE_SSL
	if (hash == NULL) 
		return true;
	if (s_hash.p == NULL) {
		if (hash != NULL) delete[] hash;
		return true;
	}
	if (s_hash.len != MD5_DIGEST_LENGTH)
		throw socketxx::error("file transfer : bad MD5 hash length");
	for (uint8_t i = 0; i < MD5_DIGEST_LENGTH; i++) {
		if (hash[i] != ((unsigned char*)s_hash.p)[i]) {
			delete[] hash;
			return false;
		}
	}
#endif
	delete[] hash;
	return true;
}
