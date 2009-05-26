//
// ccf::scoped_listen - Cluster Communication Framework
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
#ifndef CCF_SCOPED_LISTEN_H__
#define CCF_SCOPED_LISTEN_H__

#include "ccf/address.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <mp/utility.h>
#include <mp/exception.h>

namespace ccf {


class scoped_listen {
public:
	scoped_listen(const address& addr) :
		m_addr(addr),
		m_sock(listen(m_addr)) { }

	scoped_listen(struct sockaddr_in addr) :
		m_addr(addr),
		m_sock(listen(m_addr)) { }

#ifdef CCF_IPV6
	scoped_listen(struct sockaddr_in6 addr) :
		m_addr(addr),
		m_sock(listen(m_addr)) { }
#endif

	~scoped_listen()
	{
		::close(m_sock);
	}

public:
	static int listen(const address& addr)
	{
		int lsock = socket(PF_INET, SOCK_STREAM, 0);
		if(lsock < 0) {
			throw mp::system_error(errno, "socket failed");
		}
	
		int on = 1;
		if( ::setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0 ) {
			::close(lsock);
			throw mp::system_error(errno, "setsockopt failed");
		}
	
		char addrbuf[addr.addrlen()];
		addr.getaddr((sockaddr*)addrbuf);
	
		if( ::bind(lsock, (sockaddr*)addrbuf, sizeof(addrbuf)) < 0 ) {
			::close(lsock);
			throw mp::system_error(errno, "bind failed");
		}
	
		if( ::listen(lsock, 1024) < 0 ) {
			::close(lsock);
			throw mp::system_error(errno, "listen failed");
		}
	
		mp::set_nonblock(lsock);
	
		return lsock;
	}

public:
	int sock() const
	{
		return m_sock;
	}

	address addr() const
	{
		return address(m_addr);
	}

private:
	address m_addr;
	int m_sock;

private:
	scoped_listen();
	scoped_listen(const scoped_listen&);
};


}  // namespace ccf

#endif /* ccf/sopced_listen.h */

