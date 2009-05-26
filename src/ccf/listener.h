//
// ccf::listener - Cluster Communication Framework
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
#ifndef CCF_LISTENER_H__
#define CCF_LISTENER_H__

#include "ccf/util.h"
#include <sys/types.h>
#include <sys/socket.h>

//#include "log/mlogger.h"
#include <iostream>

namespace ccf {


template <typename IMPL>
class listener : public mp::wavy::handler {
public:
	listener(int fd);
	virtual ~listener();

private:
	void read_event();

	// called when new connection is established.
	//void accepted(int fd, struct sockaddr* addr, socklen_t addrlen);

	// called when the socket is closed or broken.
	//void closed();
};


template <typename IMPL>
listener<IMPL>::listener(int fd) :
	mp::wavy::handler(fd) { }

template <typename IMPL>
listener<IMPL>::~listener() { }

template <typename IMPL>
void listener<IMPL>::read_event()
try {
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);

	int nfd = ::accept(fd(), (struct sockaddr*)&addr, &addrlen);
	if(nfd <= 0) {
		if(nfd < 0) {
			if(errno == EAGAIN || errno == EINTR) {
				return;
			} else {
				static_cast<IMPL*>(this)->closed();
				throw mp::system_error(errno, "socket closed");
			}
		} else {
			static_cast<IMPL*>(this)->closed();
			throw mp::system_error(errno, "socket closed");
		}
	}

	util::fd_setup(nfd);

	try {
		static_cast<IMPL*>(this)->accepted(nfd, (struct sockaddr*)&addr, addrlen);
	} catch(...) {
		::close(nfd);
		throw;
	}

} catch(std::exception& e) {
	//FIXME LOG_ERROR("listener: ", e.what());
	std::cerr << "listener: " << e.what() << std::endl;
	throw;
} catch(...) {
	//FIXME LOG_ERROR("listener: unknown error");
	std::cerr << "listener: unknown error" << std::endl;
	throw;
}


}  // namespace ccf

#endif /* ccf/listener.h */

