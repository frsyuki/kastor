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
#ifndef OSTORAGE_HTTP_H__
#define OSTORAGE_HTTP_H__

#include "server/ostorage.h"
#include "server/service_listener.h"
#include "server/http_handler.h"

namespace kastor {


class ostorage_http : public http_handler<ostorage_http> {
public:
	ostorage_http(int fd);
	~ostorage_http();

	void process_get(const char* path, size_t pathlen, headers_t& h);

	void process_put(const char* path, size_t pathlen, headers_t& h,
			size_t content_length);

	void process_data(handler_stream s, size_t* content_length);

private:
	typedef ostorage::scoped_block scoped_block;

	scoped_block m_block;
	std::string m_key;

	char* m_map;
	size_t m_map_off;

	void reset_map();
	void reset_map(const char* path, size_t pathlen, size_t content_length);

private:
	ostorage_http();
	ostorage_http(const ostorage_http&);
};


typedef service_listener<ostorage_http> ostorage_http_listener;


}  // namespace kastor

#endif /* ostorage_http.h */

