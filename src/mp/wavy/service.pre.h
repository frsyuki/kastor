//
// mp::wavy
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

#ifndef MP_WAVY_SERVICE_H__
#define MP_WAVY_SERVICE_H__

#include "mp/wavy/core.h"
#include "mp/wavy/net.h"

namespace mp {
namespace wavy {


template <typename Instance>
struct service {

	typedef core::handler handler;
	typedef net::finalize_t finalize_t;
	typedef net::xfer xfer;

	static void init(size_t rthreads = 0, size_t wthreads = 0);

	static void add_rthread(size_t num);
	static void add_wthread(size_t num);

	static void join();
	static void detach();
	static void end();


	typedef core::connect_callback_t connect_callback_t;
	static void connect(
			int socket_family, int socket_type, int protocol,
			const sockaddr* addr, socklen_t addrlen,
			int timeout_msec, connect_callback_t callback);


	typedef core::listen_callback_t listen_callback_t;
	static void listen(int lsock, listen_callback_t callback);


	typedef core::timer_callback_t timer_callback_t;
	static void timer(const timespec* interval, timer_callback_t callback);


	static void send(int sock, const void* buf, size_t count);

	static void send(int sock, const void* buf, size_t count,
			finalize_t fin, void* user);

	static void send(int sock, const struct iovec* vec, size_t veclen,
			finalize_t fin, void* user);

	static void send(int sock, int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	static void send(int sock,
			const void* header, size_t header_len,
			int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	static void send(int sock,
			const struct iovec* header_vec, size_t header_veclen,
			int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	static void send(int sock, xfer* xf);


	template <typename Handler>
	static Handler* add(int fd);
MP_ARGS_BEGIN
	template <typename Handler, MP_ARGS_TEMPLATE>
	static Handler* add(int fd, MP_ARGS_PARAMS);
MP_ARGS_END

	template <typename F>
	static void submit(F f);
MP_ARGS_BEGIN
	template <typename F, MP_ARGS_TEMPLATE>
	static void submit(F f, MP_ARGS_PARAMS);
MP_ARGS_END

private:
	static core* s_core;
	static net* s_net;

	service();
};


template <typename Instance>
core* service<Instance>::s_core;

template <typename Instance>
net* service<Instance>::s_net;

template <typename Instance>
void service<Instance>::init(size_t rthreads, size_t wthreads)
{
	s_core = new core();
	s_net = new net();
	add_rthread(rthreads);
	add_wthread(wthreads);
}

template <typename Instance>
void service<Instance>::add_rthread(size_t num)
	{ s_core->add_thread(num); }

template <typename Instance>
void service<Instance>::add_wthread(size_t num)
	{ s_net->add_thread(num); }

template <typename Instance>
void service<Instance>::join()
{
	s_core->join();
	s_net->join();
}

template <typename Instance>
void service<Instance>::detach()
{
	s_core->detach();
	s_net->detach();
}

template <typename Instance>
void service<Instance>::end()
{
	s_core->end();
	s_net->end();
}


template <typename Instance>
inline void service<Instance>::connect(
		int socket_family, int socket_type, int protocol,
		const sockaddr* addr, socklen_t addrlen,
		int timeout_msec, connect_callback_t callback)
{
	s_core->connect(socket_family, socket_type, protocol,
			addr, addrlen, timeout_msec, callback);
}


template <typename Instance>
inline void service<Instance>::listen(int lsock, listen_callback_t callback)
	{ s_core->listen(lsock, callback); }


template <typename Instance>
inline void service<Instance>::timer(
		const timespec* interval, timer_callback_t callback)
	{ s_core->timer(interval, callback); }


template <typename Instance>
inline void service<Instance>::send(int sock, const void* buf, size_t count)
	{ s_net->send(sock, buf, count); }

template <typename Instance>
inline void service<Instance>::send(int sock, const void* buf, size_t count,
		finalize_t fin, void* user)
	{ s_net->send(sock, buf, count, fin, user); }

template <typename Instance>
inline void service<Instance>::send(int sock, const struct iovec* vec, size_t veclen,
		finalize_t fin, void* user)
	{ s_net->send(sock, vec, veclen, fin, user); }

template <typename Instance>
inline void service<Instance>::send(int sock, int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
	{ s_net->send(sock, fd, offset, count, fin, user); }

template <typename Instance>
inline void service<Instance>::send(int sock,
		const void* header, size_t header_len,
		int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
	{ s_net->send(sock, header, header_len, fd, offset, count, fin, user); }

template <typename Instance>
inline void service<Instance>::send(int sock,
		const struct iovec* header_vec, size_t header_veclen,
		int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
	{ s_net->send(sock, header_vec, header_veclen, fd, offset, count, fin, user); }

template <typename Instance>
inline void service<Instance>::send(int sock, xfer* xf)
	{ s_net->send(sock, xf); }



template <typename Instance>
template <typename Handler>
inline Handler* service<Instance>::add(int fd)
	{ return s_core->add<Handler>(fd); }
MP_ARGS_BEGIN
template <typename Instance>
template <typename Handler, MP_ARGS_TEMPLATE>
inline Handler* service<Instance>::add(int fd, MP_ARGS_PARAMS)
	{ return s_core->add<Handler, MP_ARGS_TYPES>(fd, MP_ARGS_FUNC); }
MP_ARGS_END

template <typename Instance>
template <typename F>
inline void service<Instance>::submit(F f)
	{ s_core->submit<F>(f); }
MP_ARGS_BEGIN
template <typename Instance>
template <typename F, MP_ARGS_TEMPLATE>
inline void service<Instance>::submit(F f, MP_ARGS_PARAMS)
	{ s_core->submit<F, MP_ARGS_TYPES>(f, MP_ARGS_FUNC); }
MP_ARGS_END


}  // namespace wavy
}  // namespace mp

#endif /* mp/wavy/service.h */

