//
// Kastor
//
// Copyright (C) 2009 FURUHASHI Sadayuki
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#ifndef HTTP_HANDLER_H_IMPL__
#define HTTP_HANDLER_H_IMPL__

#include <mp/exception.h>
#include <stdlib.h>
#include <iostream>

#ifndef HTTP_RESERVE_SIZE
#define HTTP_RESERVE_SIZE 1024
#endif

namespace kastor {


inline handler_stream::handler_stream(int fd, mp::stream_buffer& buffer) :
	m_fd(fd), m_buffer(buffer) { }

inline handler_stream::~handler_stream() { }

inline ssize_t handler_stream::read(void* buf, size_t count)
{
	size_t sz = m_buffer.data_size();
	if(sz > 0) {
		sz = std::min(sz, count);
		memcpy(buf, m_buffer.data(), sz);
		m_buffer.data_used(sz);
		return sz;
	} else {
		// FIXME readv() if count < X
		return ::read(m_fd, buf, count);
	}
}


template <typename IMPL>
http_handler<IMPL>::http_handler(int fd) :
	mp::wavy::handler(fd), m_content_length(0) { }

template <typename IMPL>
http_handler<IMPL>::~http_handler() { }


template <typename IMPL>
void http_handler<IMPL>::read_event()
try {
	if(m_content_length > 0) {
		static_cast<IMPL*>(this)->process_data(
				handler_stream(fd(), m_buffer), &m_content_length);
		return;
	}

	m_buffer.reserve_buffer(HTTP_RESERVE_SIZE);

	size_t rl = ::read(fd(), m_buffer.buffer(), m_buffer.buffer_capacity());
	if(rl <= 0) {
		if(rl == 0) {
			throw mp::system_error(errno, "connection closed");
		}
		if(errno == EAGAIN || errno == EINTR) {
			return;
		} else {
			throw mp::system_error(errno, "read error");
		}
	}

	m_buffer.buffer_consumed(rl);

	//std::cout << "read" << std::endl;
	//std::cout.write((const char*)m_buffer.data(), m_buffer.data_size());
	//std::cout << std::endl;

	static_cast<IMPL*>(this)->process_header();

} catch(std::exception& e) {
	//FIXME LOG_ERROR("listener: ", e.what());
	std::cerr << "listener: " << e.what() << std::endl;
	throw;
} catch(...) {
	//FIXME LOG_ERROR("listener: unknown error");
	std::cerr << "listener: unknown error" << std::endl;
	throw;
}


// FIXME extreamly ad-hoc http header parser
static char* memmem(char* p, size_t count, const void* s, size_t n)
{
	if(count < n) { return NULL; }
	char* const pend = p + count - n + 1;
	while(p < pend) {
		if(memcmp(p, s, n) == 0) {
			return p;
		}
		++p;
	}
	return NULL;
}

static char* strncasestr(char* p, size_t count, const char* needle)
{
	size_t n = strlen(needle);
	if(count < n) { return NULL; }
	char* const pend = p + count - n + 1;
	while(p < pend) {
		if(strncasecmp(p, needle, n) == 0) {
			return p;
		}
		++p;
	}
	return NULL;
}

template <typename IMPL>
void http_handler<IMPL>::process_header()
{
	// GET / HTTP/1.0\r\n\r\n
	if(m_buffer.data_size() < 18) { return; }

	char* h = (char*)m_buffer.data();

	char* posbody = memmem(h+14, m_buffer.data_size()-14, "\r\n\r\n", 4);
	if(!posbody) { return; }

	char* path = h + 4;

	char* pathend = memmem(path, posbody-path, " HTTP/", 6);
	if(!pathend) { throw std::runtime_error("invalid request"); }
	*pathend = '\0';

	if(strncasecmp(h, "GET", 3) == 0) {
		m_buffer.data_used(posbody - h + 4);

		headers_t h;
		static_cast<IMPL*>(this)->process_get(path, pathend-path, h);

	} else if(strncasecmp(h, "PUT", 3) == 0) {
		char* clen = strncasestr(pathend, posbody-pathend, "Content-Length: ");
		if(!clen) { throw std::runtime_error("invalid request"); }

		m_content_length = ::atol(clen+16);

		m_buffer.data_used(posbody - h + 4);

		headers_t h;
		static_cast<IMPL*>(this)->process_put(path, pathend-path, h, m_content_length);

	} else {
		std::cout << h << std::endl;
		throw std::runtime_error("unknown request");
	}
}


}  // namespace kastor

#endif /* http_handler_impl.h */

