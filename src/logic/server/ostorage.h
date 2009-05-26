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
#ifndef OSTORAGE_H__
#define OSTORAGE_H__

#include <stddef.h>
#include <stdint.h>
#include <mp/pthread.h>
#include <mp/exception.h>
#include <string>
#include <map>
#include <tchdb.h>
#include "clock.h"

namespace kastor {


class ostorage {
public:
	ostorage(const std::string& storage_dir);
	~ostorage();

public:
	typedef uint64_t vecoff_t;
	typedef volatile unsigned int refcount_t;

	class block {
	public:
		uint32_t size()   const { return m_size; }
		vecoff_t offset() const { return m_off;  }
		int fd() { return m_self->fd(); }

	private:
		uint32_t  m_size;
		vecoff_t  m_off;
		ostorage* m_self;
		bool is_free_block;
		refcount_t m_refcount;
		friend class ostorage;
	};

	class scoped_block;

public:
	block* balloc(uint32_t size);

	block* read(std::string key);

	bool update(std::string key, block* bk, ClockTime ct);

	bool remove(std::string key, ClockTime ct);

	static void bfree(block* bk)
	{
		if(bk) {
			bk->m_self->bfree_real(bk);
		}
	}

	int fd() const { return m_vec_fd; }

private:
	void expand_storage(vecoff_t req);

	void add_free_pool(vecoff_t off, uint32_t size);

	void bfree_real(block* bk);

private:
	// used offset
	volatile vecoff_t* m_used;

	// block lease map
	struct lease_entry {
		block* bk;
		ClockTime clocktime;
		inline lease_entry(block*, ClockTime);
	};

	// FIXME sharding
	mp::pthread_mutex m_leases_mutex;

	// FIXME hash
	typedef std::map<std::string, lease_entry> leases_t;
	leases_t m_leases;


	void* m_header_map;


	// offset => map tree
	/*
	typedef std::map<vecoff_t, void*> vec_map_t;
	volatile vecoff_t m_maped_size;
	vec_map_t m_vec_map;

	mp::pthread_mutex m_vec_map_mutex;
	*/


	// index hash map;
	TCHDB* m_index_map;

	// free block pool;
	// size -> offset tree
	/*TCBDB* m_free_map;*/

	// storage vector;
	int m_vec_fd;
	mp::pthread_mutex m_storage_mutex;
	vecoff_t m_vec_size;

private:
	ostorage();
	ostorage(const ostorage&);
};


class ostorage::scoped_block {
public:
	block* operator->() const { return m; }
	block& operator*() const { return *m; }
	block* get() const { return m; }

	operator bool() const { return m != NULL; }

	block* release()
	{
		block* tmp = m;
		m = NULL;
		return tmp;
	}

	void reset(block* bk = NULL)
	{
		if(m) ostorage::bfree(m);
		m = bk;
	}

	scoped_block(block* bk = NULL) : m(bk) { }

	~scoped_block() { reset(); }

private:
	block* m;

	scoped_block(const scoped_block&);
};


}  // namespace kastor

#endif /* ostorage.h */

