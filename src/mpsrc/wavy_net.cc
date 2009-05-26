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

#include "mp/wavy/net.h"
#include "mp/pthread.h"
#include "wavy_edge.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/sendfile.h>
#endif

#ifndef MP_WAVY_TASK_QUEUE_LIMIT
#define MP_WAVY_TASK_QUEUE_LIMIT 16
#endif

/* FIXME
#ifndef MP_WAVY_WRITEV_LIMIT
#define MP_WAVY_WRITEV_LIMIT 1024
#endif
*/

//#ifndef MP_WAVY_WRITE_QUEUE_LIMIT
//#define MP_WAVY_WRITE_QUEUE_LIMIT 32
//#endif

namespace mp {
namespace wavy {


class net::impl {
public:
	impl();
	~impl();

	void add_thread(size_t num);

	void end();
	bool is_end() const;

	void join();
	void detach();

public:
	inline void send(int sock, const void* buf, size_t count);

	inline void send(int sock, const void* buf, size_t count,
			finalize_t fin, void* user);

	inline void send(int sock, const struct iovec* vec, size_t veclen,
			finalize_t fin, void* user);

	inline void send(int sock, int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	inline void send(int sock,
			const void* header, size_t header_len,
			int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	inline void send(int sock,
			const struct iovec* header_vec, size_t header_veclen,
			int fd, uint64_t offset, size_t count,
			finalize_t fin, void* user);

	inline void send(int sock, xfer* xf);

	inline void submit_impl(task_t& f);

public:
	void operator() ();

private:
	// FIXME advanced vector, netvec
	class context : public xfer {
	public:
		context();
		~context();

	public:
		bool write_event(int fd);
		bool try_write(int fd);

		pthread_mutex& mutex();
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
		void wait_cond();
		size_t size();
#endif

	private:
		pthread_mutex m_mutex;
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
		pthread_cond m_cond;
		volatile bool m_wait;
#endif

	private:
		context(const context&);
	};

	void send_post(context& ctx, int fd, bool xempty);

private:
	volatile size_t m_off;
	volatile size_t m_num;
	volatile bool m_pollable;

	edge::backlog m_backlog;

	context* m_fdctx;

	edge m_edge;

	pthread_mutex m_mutex;
	pthread_cond m_cond;

	volatile bool m_end_flag;

	typedef std::queue<task_t> task_queue_t;
	task_queue_t m_task_queue;

private:
	typedef std::vector<pthread_thread*> workers_t;
	workers_t m_workers;

private:
	impl(const impl&);
};


inline void net::xfer::reserve(size_t reqsz)
{
	size_t used = m_tail - m_head;
	reqsz += used;
	size_t nsize = (used + m_free) * 2 + 72;  // used + m_free may be 0

	while(nsize < reqsz) { nsize *= 2; }

	char* tmp = (char*)::realloc(m_head, nsize);
	if(!tmp) { throw std::bad_alloc(); }

	m_head = tmp;
	m_tail = tmp + used;
	m_free = nsize - used;
}

void net::xfer::push_iov(const struct iovec* vec, size_t veclen)
{
	size_t vecbuflen = sizeof(struct iovec) * veclen;
	size_t reqsz = vecbuflen + sizeof(size_t) + sizeof(xfer_type);
	if(m_free < reqsz) { reserve(reqsz); }

	*(xfer_type*)m_tail = XF_IOV;
	m_tail += sizeof(xfer_type);

	*(size_t*)m_tail = veclen;
	m_tail += sizeof(size_t);

	memcpy(m_tail, vec, vecbuflen);
	m_tail += vecbuflen;

	m_free -= reqsz;
}

void net::xfer::push_file(int fd, uint64_t off, size_t len)
{
	size_t reqsz = sizeof(xfer_file) + sizeof(xfer_type);
	if(m_free < reqsz) { reserve(reqsz); }

	xfer_file x = {fd, off, len};

	*(xfer_type*)m_tail = XF_FILE;
	m_tail += sizeof(xfer_type);

	memcpy(m_tail, &x, sizeof(xfer_file));
	m_tail += sizeof(xfer_file);

	m_free -= reqsz;
}

void net::xfer::push_finalize(void (*finalize)(void*), void* user)
{
	size_t reqsz = sizeof(xfer_file) + sizeof(xfer_type);
	if(m_free < reqsz) { reserve(reqsz); }

	xfer_finalize x = {finalize, user};

	*(xfer_type*)m_tail = XF_FINALIZE;
	m_tail += sizeof(xfer_type);

	memcpy(m_tail, &x, sizeof(xfer_finalize));
	m_tail += sizeof(xfer_finalize);

	m_free -= reqsz;
}

void net::xfer::migrate(xfer* target)
{
	size_t reqsz = m_tail - m_head;
	if(target->m_free < reqsz) {
		target->reserve(reqsz);
	}

	memcpy(target->m_tail, m_head, reqsz);

	target->m_tail += reqsz;
	target->m_free -= reqsz;

	::free(m_head);
	m_head = NULL;
	m_tail = NULL;
	m_free = 0;
}

void net::xfer::reset()
{
	for(char* p = m_head; p < m_tail; ) {
		switch( *(xfer_type*)p ) {
		case XF_IOV:
			p += sizeof(xfer_type) + sizeof(size_t) +
				sizeof(struct iovec) * (*(size_t*)(p + sizeof(xfer_type)));
			break;

		case XF_FILE:
			p += sizeof(xfer_type) + sizeof(xfer_file);
			break;

#if 0
		case XF_MEM:
			p += sizeof(xfer_type) + sizeof(size_t) +
				(*(size_t*)(p + sizeof(xfer_type)));
#endif

		case XF_FINALIZE:
			try {
				xfer_finalize* x = (xfer_finalize*)(p + sizeof(xfer_type));
				x->finalize(x->user);
			} catch (...) { }
			p += sizeof(xfer_type) + sizeof(xfer_finalize);
			break;
		}
	}

	m_free += m_tail - m_head;
	m_tail = m_head;
}



net::net() : m_impl(new impl()) { }

net::impl::impl() :
	m_off(0),
	m_num(0),
	m_pollable(true),
	m_end_flag(false)
{
	struct rlimit rbuf;
	if(::getrlimit(RLIMIT_NOFILE, &rbuf) < 0) {
		throw system_error(errno, "getrlimit() failed");
	}
	m_fdctx = new context[rbuf.rlim_cur];
}


net::~net() { }

net::impl::~impl()
{
	end();
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
	}
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		delete *it;
	}
	delete[] m_fdctx;
}


void net::end() { m_impl->end(); }
void net::impl::end()
{
	m_end_flag = true;
	{
		pthread_scoped_lock lk(m_mutex);
		m_cond.broadcast();
	}
}

bool net::is_end() const { return m_impl->is_end(); }
bool net::impl::is_end() const
{
	return m_end_flag;
}

void net::join() { m_impl->join(); }
void net::impl::join()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->join();
	}
}

void net::detach() { m_impl->detach(); }
void net::impl::detach()
{
	for(workers_t::iterator it(m_workers.begin());
			it != m_workers.end(); ++it) {
		(*it)->detach();
	}
}

void net::add_thread(size_t num) { m_impl->add_thread(num); }
void net::impl::add_thread(size_t num)
{
	for(size_t i=0; i < num; ++i) {
		m_workers.push_back(NULL);
		try {
			m_workers.back() = new pthread_thread(this);
		} catch (...) {
			m_workers.pop_back();
			throw;
		}
		m_workers.back()->run();
	}
}


void net::impl::submit_impl(task_t& f)
{
	pthread_scoped_lock lk(m_mutex);
	m_task_queue.push(f);
	m_cond.signal();
}
void net::submit_impl(task_t f)
	{ m_impl->submit_impl(f); }


net::impl::context::context()
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
	: m_wait(false)
#endif
	{ }

net::impl::context::~context() { }

inline pthread_mutex& net::impl::context::mutex()
{
	return m_mutex;
}

#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
inline void net::impl::context::wait_cond()
{
	m_wait = true;
	m_cond.wait(m_mutex);
}

inline size_t net::impl::context::size() const
{
	return m_tail - m_head;
}
#endif

inline bool net::impl::context::write_event(int fd)
{
	pthread_scoped_lock lk(m_mutex);
	return try_write(fd);
}

#define MPIO_NET_XFER_CONSUMED \
	do { \
		size_t trail = m_tail - p; \
		::memmove(m_head, p, trail); \
		m_tail = m_head + trail; \
	} while(0)

bool net::impl::context::try_write(int fd)
{
	char* p = m_head;
	while(p < m_tail) {
		switch( *(xfer_type*)p ) {
		case XF_IOV: {
			size_t veclen = *(size_t*)(p + sizeof(xfer_type));
			struct iovec* vec = (struct iovec*)(p + sizeof(xfer_type) + sizeof(size_t));

			ssize_t wl = ::writev(fd, vec, veclen);
			if(wl <= 0) {
				MPIO_NET_XFER_CONSUMED;
				if(wl == 0) { return false; }
				if(errno == EAGAIN || errno == EINTR) {
					return true;
				} else {
					::shutdown(fd, SHUT_RD);
					return false;
				}
			}

			for(size_t i=0; i < veclen; ++i) {
				if(static_cast<size_t>(wl) >= vec[i].iov_len) {
					wl -= vec[i].iov_len;
				} else {
					vec[i].iov_base = (void*)(((char*)vec[i].iov_base) + wl);
					vec[i].iov_len -= wl;

					if(i > 0) {
						*(xfer_type*)m_head = XF_IOV;
						*(size_t*)(m_head + sizeof(xfer_type)) = veclen - i;

						p += sizeof(xfer_type) + sizeof(size_t) + sizeof(struct iovec)*i;

						size_t trail = m_tail - p;
						::memmove(m_head + sizeof(xfer_type), p, trail);
						m_tail = m_head + sizeof(xfer_type) + trail;

					} else {
						MPIO_NET_XFER_CONSUMED;
					}

					return true;
				}
			}

			p += sizeof(xfer_type) + sizeof(size_t) + sizeof(struct iovec) * veclen;

			break; }

		case XF_FILE: {
			xfer_file* x = (xfer_file*)(p + sizeof(xfer_type));

#ifdef __linux__
			off_t off = x->off;
			ssize_t wl = ::sendfile(fd, x->fd, &off, x->len);
			if(wl <= 0) {
				MPIO_NET_XFER_CONSUMED;
				if(wl == 0) { return false; }
				if(errno == EAGAIN || errno == EINTR) {
					return true;
				} else {
					::shutdown(fd, SHUT_RD);
					return false;
				}
			}
#else
			off_t wl = x->size;
			if(::sendfile(x->fd, fd, x->off, &wl, NULL, 0) < 0) {
				if(errno == EAGAIN || errno == EINTR) {
					return true;
				} else {
					::shutdown(fd, SHUT_RD);
					return false;
				}
			}
#endif

			if(static_cast<size_t>(wl) < x->len) {
				x->off += wl;
				x->len -= wl;
				MPIO_NET_XFER_CONSUMED;
				return true;
			}

			p += sizeof(xfer_type) + sizeof(xfer_file);

			break; }

#if 0
		case XF_MEM: {
			size_t size = *(size_t*)(p + sizeof(xfer_type));
			void* buf = p + sizeof(xfer_type) + sizeof(size_t);

			ssize_t wl = ::write(fd, buf, size);
			if(wl <= 0) {
				MPIO_NET_XFER_CONSUMED;
				if(wl == 0) { return false; }
				if(errno == EAGAIN || errno == EINTR) {
					return true;
				} else {
					::shutdown(fd, SHUT_RD);
					return false;
				}
			}

			if(static_cast<size_t>(wl) < size) {
				*(xfer_type*)m_head = XF_IOV;
				*(size_t*)(m_head + sizeof(xfer_type)) = size - wl;

				p += sizeof(xfer_type) + sizeof(size_t) + wl;

				size_t trail = m_tail - p;
				::memmove(m_head + sizeof(xfer_type), p, trail);
				m_tail = m_head + sizeof(xfer_type) + trail;

				return true;
			}

			p += sizeof(xfer_type) + sizeof(size_t) + size;

			break; }
#endif

		case XF_FINALIZE:
			try {
				xfer_finalize* x = (xfer_finalize*)(p + sizeof(xfer_type));
				x->finalize(x->user);
			} catch (...) { }

			p += sizeof(xfer_type) + sizeof(xfer_finalize);

			break;
		}
	}

	m_free += m_tail - m_head;
	m_tail = m_head;

	return false;
}


void net::impl::operator() ()
{
	retry:
	while(true) {
		pthread_scoped_lock lk(m_mutex);

		while(m_task_queue.size() > MP_WAVY_TASK_QUEUE_LIMIT || !m_pollable) {
			if(m_end_flag) { return; }

			if(!m_task_queue.empty()) {
				task_t ev = m_task_queue.front();
				m_task_queue.pop();
				if(!m_task_queue.empty()) { m_cond.signal(); }
				lk.unlock();
				try {
					ev();
				} catch (...) { }
				goto retry;
			}

			m_cond.wait(m_mutex);
		}

		if(m_num == m_off) {
			m_pollable = false;
			lk.unlock();

		retry_poll:
			if(m_end_flag) { return; }

			int num = m_edge.wait(&m_backlog, 1000);
			if(num < 0) {
				if(errno == EINTR || errno == EAGAIN) {
					goto retry_poll;
				} else {
					throw system_error(errno, "wavy net event failed");
				}
			} else if(num == 0) {
				goto retry_poll;
			}

			lk.relock(m_mutex);
			m_off = 0;
			m_num = num;

			m_pollable = true;
			m_cond.signal();
		}

		int fd = m_backlog[m_off];
		++m_off;
		lk.unlock();

		{
			bool cont;
			try {
				cont = m_fdctx[fd].write_event(fd);
			} catch (...) { cont = false; }
	
			if(!cont) {
				m_edge.shot_remove(fd, EVEDGE_WRITE);
				m_fdctx[fd].reset();
				goto retry;
			}
		}

		m_edge.shot_reactivate(fd, EVEDGE_WRITE);
	}
}


inline void net::impl::send_post(context& ctx, int fd, bool xempty)
{
	if(xempty) {
#ifndef MP_WAVY_NO_TRY_WRITE_INITIAL
		bool cont;
		try {
			cont = ctx.try_write(fd);
		} catch (...) { cont = false; }

		if(cont) {
			m_edge.add_notify(fd, EVEDGE_WRITE);
		} else {
			ctx.reset();
		}
#else
		m_edge.add_notify(fd, EVEDGE_WRITE);
#endif
	} else {
#ifdef MP_WAVY_WRITE_QUEUE_LIMIT
		// FIXME sender or receiver must not wait to flush to avoid deadlock.
		// FIXME
		while(ctx.size() > MP_WAVY_WRITE_QUEUE_LIMIT) {
			ctx.wait_cond();
		}
#endif
	}
}


void net::impl::send(int sock, const void* buf, size_t count)
{
	context& ctx(m_fdctx[sock]);
	pthread_scoped_lock lk(ctx.mutex());
	bool xempty = ctx.empty();

	struct iovec vec = {(void*)buf, count};
	ctx.push_iov(&vec, 1);
	send_post(ctx, sock, xempty);
}
void net::send(int sock, const void* buf, size_t count)
	{ m_impl->send(sock, buf, count); }


void net::impl::send(int sock, const void* buf, size_t count,
		finalize_t fin, void* user)
{
	context& ctx(m_fdctx[sock]);
	pthread_scoped_lock lk(ctx.mutex());
	bool xempty = ctx.empty();

	struct iovec vec = {(void*)buf, count};
	ctx.push_iov(&vec, 1);
	if(fin) { ctx.push_finalize(fin, user); }
	send_post(ctx, sock, xempty);
}
void net::send(int sock, const void* buf, size_t count,
		finalize_t fin, void* user)
	{ m_impl->send(sock, buf, count, fin, user); }


void net::impl::send(int sock, const struct iovec* vec, size_t veclen,
		finalize_t fin, void* user)
{
	context& ctx(m_fdctx[sock]);
	pthread_scoped_lock lk(ctx.mutex());
	bool xempty = ctx.empty();

	ctx.push_iov(vec, veclen);
	if(fin) { ctx.push_finalize(fin, user); }
	send_post(ctx, sock, xempty);
}
void net::send(int sock, const struct iovec* vec, size_t veclen,
		finalize_t fin, void* user)
	{ m_impl->send(sock, vec, veclen, fin, user); }


void net::impl::send(int sock, int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
{
	context& ctx(m_fdctx[sock]);
	pthread_scoped_lock lk(ctx.mutex());
	bool xempty = ctx.empty();

	ctx.push_file(fd, offset, count);
	if(fin) { ctx.push_finalize(fin, user); }
	send_post(ctx, sock, xempty);
}
void net::send(int sock, int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
	{ m_impl->send(sock, fd, offset, count, fin, user); }


void net::impl::send(int sock,
		const void* header, size_t header_len,
		int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
{
	context& ctx(m_fdctx[sock]);
	pthread_scoped_lock lk(ctx.mutex());
	bool xempty = ctx.empty();

	struct iovec vec = {(void*)header, header_len};
	ctx.push_iov(&vec, 1);
	ctx.push_file(fd, offset, count);
	if(fin) { ctx.push_finalize(fin, user); }
	send_post(ctx, sock, xempty);
}
void net::send(int sock,
		const void* header, size_t header_len,
		int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
	{ m_impl->send(sock, header, header_len, fd, offset, count, fin, user); }


void net::impl::send(int sock,
		const struct iovec* header_vec, size_t header_veclen,
		int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
{
	context& ctx(m_fdctx[sock]);
	pthread_scoped_lock lk(ctx.mutex());
	bool xempty = ctx.empty();

	ctx.push_iov(header_vec, header_veclen);
	ctx.push_file(fd, offset, count);
	if(fin) { ctx.push_finalize(fin, user); }
	send_post(ctx, sock, xempty);
}
void net::send(int sock,
		const struct iovec* header_vec, size_t header_veclen,
		int fd, uint64_t offset, size_t count,
		finalize_t fin, void* user)
	{ m_impl->send(sock, header_vec, header_veclen, fd, offset, count, fin, user); }



void net::impl::send(int sock, xfer* xf)
{
	context& ctx(m_fdctx[sock]);
	pthread_scoped_lock lk(ctx.mutex());
	bool xempty = ctx.empty();

	xf->migrate(&ctx);
	send_post(ctx, sock, xempty);
}
void net::send(int sock, xfer* xf)
	{ m_impl->send(sock, xf); }


}  // namespace wavy
}  // namespace mp

