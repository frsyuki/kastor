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
#ifndef HTTP_HANDLER_H__
#define HTTP_HANDLER_H__

#include "server/types.h"
#include <ccf/service.h>
#include <mp/stream_buffer.h>
#include <map>
#include <string>

namespace kastor {


class handler_stream {
public:
	handler_stream(int fd, mp::stream_buffer& buffer);
	~handler_stream();

	ssize_t read(void* buf, size_t count);

private:
	int m_fd;
	mp::stream_buffer& m_buffer;

	handler_stream();
};


template <typename IMPL>
class http_handler : public mp::wavy::handler {
public:
	http_handler(int fd);
	~http_handler();

public:
	void read_event();

	void process_header();

	typedef std::map<std::string, std::string> headers_t;

	//void process_get(const char* path, size_t pathlen, headers_t& h);

	//void process_put(const char* path, size_t pathlen, headers_t& h,
	//		size_t content_length);

	//void process_data(handler_stream s, size_t* content_length);

private:
	mp::stream_buffer m_buffer;
	size_t m_content_length;

private:
	http_handler();
	http_handler(const http_handler&);
};


}  // namespace kastor

#include "http_handler_impl.h"

#endif /* http_handler.h */

