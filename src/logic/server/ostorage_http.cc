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
#include "server/ostorage_http.h"
#include "server/framework.h"
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

namespace kastor {


static const char* NOT_FOUND =
	"HTTP/1.1 404 Not Found\r\n"
	"Content-Length: 9\r\n"
	"\r\n"
	"Not Found\r\n";

static const char* CREATED =
	"HTTP/1.1 201 Created\r\n"
	"Content-Length: 7\r\n"
	"\r\n"
	"Created\r\n";

static const char* OK_FORMAT =
	"HTTP/1.1 200 OK\r\n"
	"Content-Length: %llu\r\n"
	"\r\n";

ostorage_http::ostorage_http(int fd) :
	http_handler<ostorage_http>(fd),
	m_map(NULL), m_map_off(0) { }

ostorage_http::~ostorage_http()
{
	reset_map();
}

static void buf_free(void* buf)
{
	ostorage::bfree(*(ostorage::block**)buf);
	::free(buf);
}

void ostorage_http::process_get(const char* path, size_t pathlen, headers_t& h)
{
	std::cout << "http get " << path << " " << pathlen << std::endl;
	std::string key(path, pathlen);
	scoped_block bk( net->storage().read(key) );

	if(!bk) {
		wavy::send(fd(), NOT_FOUND, strlen(NOT_FOUND), NULL, NULL);
		return;
	}

	char* buf = (char*)::malloc(
			sizeof(ostorage::block**) + strlen(OK_FORMAT)+10);

	if(!buf) { throw std::bad_alloc(); }

	*(ostorage::block**)buf = bk.get();
	char* header = buf + sizeof(ostorage::block**);

	int header_len = sprintf(header, OK_FORMAT, bk->size());

	wavy::send(fd(),
			header, header_len,
			bk->fd(), bk->offset(), bk->size(),
			&buf_free, buf);
	bk.release();
}

void ostorage_http::reset_map()
{
	if(m_map) {
		size_t map_size = (m_block->size() + m_map_off + sysconf(_SC_PAGE_SIZE)-1)
			& ~(sysconf(_SC_PAGE_SIZE)-1);
		::munmap(m_map, map_size);
		m_map = NULL;
		m_block.reset();
		m_key = "";
	}
}

void ostorage_http::reset_map(const char* path, size_t pathlen, size_t content_length)
{
	reset_map();

	m_key = std::string(path, pathlen);

	m_block.reset( net->storage().balloc(content_length) );

	m_map_off = m_block->offset()
		- (m_block->offset() / sysconf(_SC_PAGE_SIZE) * sysconf(_SC_PAGE_SIZE));

	size_t map_size = (m_block->size() + m_map_off + sysconf(_SC_PAGE_SIZE)-1)
		& ~(sysconf(_SC_PAGE_SIZE)-1);

	void* map = ::mmap(NULL, map_size, PROT_WRITE, MAP_SHARED,
			m_block->fd(), m_block->offset() - m_map_off);
	if(map == MAP_FAILED) {
		int err = errno;
		m_block.reset();
		throw mp::system_error(err, "mmap failed");
	}

	m_map = (char*)map;
}

void ostorage_http::process_put(const char* path, size_t pathlen, headers_t& h,
		size_t content_length)
{
	std::cout << "http put " << path << " " << pathlen << std::endl;
	std::cout << "clen: " << content_length << std::endl;
	reset_map(path, pathlen, content_length);
}

void ostorage_http::process_data(handler_stream s, size_t* content_length)
{
	size_t off = m_block->size() - (*content_length);

	ssize_t rl = s.read(m_map + m_map_off + off, *content_length);
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

	std::cout << "read content " << rl << std::endl;

	*content_length -= rl;

	if(*content_length == 0) {
		ClockTime clocktime( Clock(0), time(NULL) );
		net->storage().update(m_key, m_block.get(), clocktime);
		wavy::send(fd(), CREATED, strlen(CREATED), NULL, NULL);
		reset_map();
	}
}


}  // namespace kastor

