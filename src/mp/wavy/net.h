//
// mp::wavy::net
//
// Copyright (C) 2008 FURUHASHI Sadayuki
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

#ifndef MP_WAVY_NET_H__
#define MP_WAVY_NET_H__

#include "mp/functional.h"
#include "mp/memory.h"
#include "mp/pthread.h"
#include "mp/object_callback.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>
#include <queue>

namespace mp {
namespace wavy {


class net {
public:
	net();
	~net();

	void add_thread(size_t num);

	void end();
	bool is_end() const;

	void join();
	void detach();

public:
	class xfer;

	typedef void (*finalize_t)(void* user);

	void send(int sock, const void* buf, size_t count);

	void send(int sock, const void* buf, size_t count,
			finalize_t fin, void* user);

	void send(int sock, const struct iovec* vec, size_t veclen,
			finalize_t fin, void* user);

	void send(int sock, int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	void send(int sock,
			const void* header, size_t header_len,
			int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	void send(int sock,
			const struct iovec* header_vec, size_t header_veclen,
			int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	void send(int sock, xfer* xf);

/*
	template <typename F>
	void submit(F f);
MP_ARGS_BEGIN
	template <typename F, MP_ARGS_TEMPLATE>
	void submit(F f, MP_ARGS_PARAMS);
MP_ARGS_END
*/

private:
	typedef function<void ()> task_t;
	void submit_impl(task_t f);

private:
	class impl;
	const std::auto_ptr<impl> m_impl;

	net(const net&);
};


class net::xfer {
public:
	xfer();
	~xfer();

public:
	void push_iov(const struct iovec* vec, size_t veclen);

	void push_file(int fd, uint64_t off, size_t len);

	void push_finalize(void (*finalize)(void*), void* user);

	template <typename T>
	void push_finalize(std::auto_ptr<T>& fin);

	template <typename T>
	void push_finalize(mp::shared_ptr<T>& fin);

public:
	bool empty() const;

	void reset();

	void migrate(xfer* target);

protected:
	typedef enum {
		XF_IOV,
		XF_FILE,
		XF_FINALIZE,
	} xfer_type;

	struct xfer_file {
		int fd;
		uint64_t off;
		size_t len;
	};

	struct xfer_finalize {
		void (*finalize)(void*);
		void* user;
	};

	char* m_head;
	char* m_tail;
	size_t m_free;

	void reserve(size_t reqsz);

private:
	xfer(const xfer&);
};


inline net::xfer::xfer() :
	m_head(NULL), m_tail(NULL), m_free(0) { }

inline net::xfer::~xfer()
{
	if(m_head) { reset(); }
}

inline bool net::xfer::empty() const
{
	return m_head == m_tail;
}

template <typename T>
inline void net::xfer::push_finalize(std::auto_ptr<T>& fin)
{
	push_finalize(&mp::object_delete<T>, reinterpret_cast<void*>(fin.get()));
	fin.release();
}

template <typename T>
inline void net::xfer::push_finalize(mp::shared_ptr<T>& fin)
{
	std::auto_ptr<mp::shared_ptr<T> > afin(new mp::shared_ptr<T>(fin));
	push_finalize(afin);
}


}  // namespace wavy
}  // namespace mp

#endif /* mp/wavy/net.h */

