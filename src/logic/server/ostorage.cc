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
#include "ostorage.h"
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define VEC_HEADER_SIZE 4096

#define VEC_EXPAND_SIZE (2LLU*1024*1024*1024)

namespace kastor {


// storage vector header = 4KB
// magic, endian, used-size
// +---+-------+-------+
// + 8 |   6   |   8   |  ...
// +---+-------+-------+
// endian
//     (padding)
//             used size

// index value
// +---+---+-------+
// | 4 | 4 |   8   |
// +---+---+-------+
// absolute time
//     size
//         offset
// removed if offset == 0


inline ostorage::lease_entry::lease_entry(block* b, ClockTime ct) :
	bk(b), clocktime(ct) { }


ostorage::ostorage(const std::string& storage_dir)
{
	int err = 0;

	std::string vec_path   = storage_dir + "/vector";
	std::string index_path = storage_dir + "/index.tch";
	//std::string free_path  = storage_dir + "/free.tch";

	struct stat stbuf;

	m_vec_fd = ::open(vec_path.c_str(), O_RDWR|O_CREAT, 0666);
	if(m_vec_fd < 0) {
		err = errno; goto out_storage;
	}

	// FIXME flock

	if(fstat(m_vec_fd, &stbuf) < 0) {
		err = errno; goto out_storage;
	}

	m_vec_size = stbuf.st_size;
	if(m_vec_size == 0) {
		m_vec_size = VEC_HEADER_SIZE;
		if(ftruncate(m_vec_fd, m_vec_size) < 0) {
			err = errno; goto out_storage;
		}
	} else if(m_vec_size < VEC_HEADER_SIZE) {
		err = errno; goto out_storage;  // FIXME
	}

	m_header_map = ::mmap(NULL, VEC_HEADER_SIZE, PROT_READ|PROT_WRITE,
			MAP_SHARED, m_vec_fd, 0);
	if(m_header_map == MAP_FAILED) {
		err = errno; goto out_header_mmap;
	}

	// FIXME check endiang
	m_used = (volatile uint64_t*)(((char*)m_header_map) + 8);

	m_index_map = tchdbnew();
	if(!m_index_map) {
		err = errno; goto out_index_map;
	}

	if(!tchdbopen(m_index_map, index_path.c_str(), HDBOWRITER|HDBOCREAT)) {
		err = errno; goto out_index_map_open;
	}

	/*
	m_free_map = tchdbnew();
	if(!m_free_map) {
		err = errno; goto out_free_map;
	}

	if(!tchdbopen(m_free_map, free_path.c_str(), HDBOWRITER|HDBOCREAT)) {
		err = errno; goto out_free_map_open;
	}
	*/

	return;

	/*tchdbclose(m_free_map);*/

	/*
out_free_map_open:
	tchdbdel(m_free_map);

out_free_map:
	*/
	/*tchdbclose(m_index_map);*/

out_index_map_open:
	tchdbdel(m_index_map);

out_index_map:
	::munmap(m_header_map, VEC_HEADER_SIZE);

out_header_mmap:
	::close(m_vec_fd);

out_storage:
	throw mp::system_error(err, "can't initialize storage");
}

ostorage::~ostorage()
{
	// close, munmap, ...
	mp::pthread_scoped_lock lslk(m_leases_mutex);
	for(leases_t::iterator it(m_leases.begin()),
			it_end(m_leases.end()); it != it_end; ++it) {
		block* bk = it->second.bk;
		if(bk) try {
			bfree_real(bk);
		} catch (...) { }
	}

	/*
	tchdbclose(m_free_map);
	tchdbdel(m_free_map);
	*/
	tchdbclose(m_index_map);
	tchdbdel(m_index_map);
	::munmap(m_header_map, VEC_HEADER_SIZE);
	::close(m_vec_fd);
	/*
	for(vec_map_t::iterator it(m_vec_map.begin()),
			it_end(m_vec_map.end()); it != it_end; ++it) {
		::munmap(it->second, VEC_EXPAND_SIZE);
	}
	*/
}

void ostorage::expand_storage(vecoff_t req)
{
	mp::pthread_mutex m_storage_mutex;
	if(m_vec_size >= req) { return; }

	vecoff_t nsize = m_vec_size + VEC_EXPAND_SIZE;
	while(nsize < req) { nsize += VEC_EXPAND_SIZE; }

	if(ftruncate(m_vec_fd, nsize) < 0) {
		throw mp::system_error(errno, "ftruncate");
	}
	m_vec_size = nsize;
}

void ostorage::add_free_pool(vecoff_t off, uint32_t size)
{
	// FIXME not implemented yet
}


ostorage::block* ostorage::balloc(uint32_t size)
{
	block* bk = (block*)::malloc(sizeof(block));
	if(!bk) {
		throw std::bad_alloc();
	}

	vecoff_t off = __sync_fetch_and_add(m_used, size);
	if(m_vec_size < off+size) try {
		expand_storage(off+size);
	} catch (...) {
		::free(bk);
		throw;
	}

	bk->m_size = size;
	bk->m_off  = off;
	bk->m_self = this;
	bk->m_refcount = 1;
	bk->is_free_block = true;

	return bk;
}

void ostorage::bfree_real(block* bk)
{
	if(__sync_sub_and_fetch(&bk->m_refcount, 1) == 0) {
		if(bk->is_free_block) {
			uint32_t size = bk->size();
			vecoff_t off = bk->offset();
			::free(bk);

			add_free_pool(off, size);

		} else {
			::free(bk);
		}
	}
}

ostorage::block* ostorage::read(std::string key)
{
	mp::pthread_scoped_lock lslk(m_leases_mutex);
	leases_t::iterator it = m_leases.find(key);
	if(it != m_leases.end()) {
		block* bk = it->second.bk;
		if(bk) {
			__sync_fetch_and_add(&bk->m_refcount, 1);
			return bk;
		} else {
			return NULL;
		}
	}

	int memlen;
	void* mem = tchdbget(m_index_map, key.data(), key.size(), &memlen);
	if(!mem) {
		return NULL;
	} else if(memlen < 16) {
		::free(mem);
		return NULL;
	}

	uint32_t time = *(uint32_t*)mem;                   // FIXME endian
	uint32_t size = *(uint32_t*)(((char*)mem) + 4);    // FIXME endian
	vecoff_t off  = *(vecoff_t*)(((char*)mem) + 8);    // FIXME endian

	::free(mem);

	// FIXME invalid clock
	std::pair<leases_t::iterator, bool> ins = m_leases.insert(
			leases_t::value_type(key, lease_entry(NULL, ClockTime(Clock(), time))) );

	if(!ins.second) { return NULL; }

	block* bk = (block*)::malloc(sizeof(block));
	if(!bk) {
		m_leases.erase(ins.first);
		throw std::bad_alloc();
	}
	ins.first->second.bk = bk;

	bk->m_size = size;
	bk->m_off  = off;
	bk->m_self = this;
	bk->m_refcount = 2;  // lease_entry + return
	bk->is_free_block = false;

	return bk;
}


namespace {
	typedef ostorage::vecoff_t vecoff_t;

	static inline void update_casproc_set_ignored(char* mem)
	{
		*(uint32_t*)mem = 0;
	}
	
	static inline bool update_casproc_is_ignored(char* mem)
	{
		return *(uint32_t*)mem == 0;
	}
	
	static inline void update_casproc_set_swapped(char* newmem, const char* oldmem)
	{
		*(uint32_t*)(newmem + 4) = *(uint32_t*)(oldmem + 4);
		*(vecoff_t*)(newmem + 8) = *(vecoff_t*)(oldmem + 8);
	}
	
	static inline void update_casproc_unset_swapped(char* mem)
	{
		*(uint32_t*)(mem + 4) = 0;
		*(vecoff_t*)(mem + 8) = 0;
	}
	
	static inline bool update_casproc_is_swapped(char* mem)
	{
		return *(vecoff_t*)(mem + 8) != 0;
	}
	
	static void* update_casproc(const void* vbuf, int vsiz, int* sp, void* op)
	{
		char* mem = (char*)op;
	
		if(vsiz == 16) {
			uint32_t time = *(uint32_t*)vbuf;    // FIXME endian
			uint32_t castime = *(uint32_t*)mem;  // FIXME endian
			if(castime < time) {      // FIXME time compare
				update_casproc_set_ignored(mem);
				return NULL;
			}
		}
	
		char* buf = (char*)malloc(16);
		if(!buf) {
			update_casproc_set_ignored(mem);
			return NULL;
		}
		memcpy(buf, mem, 16);
	
		if(vsiz == 16) {
			update_casproc_set_swapped(mem, (const char*)vbuf);
		} else {
			update_casproc_unset_swapped(mem);
		}
	
		return buf;
	}
}  // noname namespace

bool ostorage::update(std::string key, block* bk, ClockTime ct)
{
	char mem[16];
	*(uint32_t*)mem                = ct.time();     // FIXME endian
	*(uint32_t*)(((char*)mem) + 4) = bk->size();    // FIXME endian
	*(vecoff_t*)(((char*)mem) + 8) = bk->offset();  // FIXME endian

	mp::pthread_scoped_lock lslk(m_leases_mutex);
	leases_t::iterator it = m_leases.find(key);
	if(it != m_leases.end()) {
		if(ct < it->second.clocktime) {
			// FIXME invalid clock, ranged compare, vector clock
			return false;
		}

		// time is checked
		if(!tchdbput(m_index_map,
				key.data(), key.size(),
				mem, sizeof(mem))) {
			return false;  // FIXME exception?
		}

		if(it->second.bk) {
			it->second.bk->is_free_block = true;
			bfree_real(it->second.bk);
		}

		__sync_fetch_and_add(&bk->m_refcount, 1);
		it->second.bk = bk;
		it->second.clocktime = ct;

		bk->is_free_block = false;

		return true;

	} else {
		if(!tchdbputproc(m_index_map,
				key.data(), key.size(),
				mem, sizeof(mem),
				update_casproc, (void*)mem)) {
			return false;  // FIXME exception?
		}

		if(update_casproc_is_ignored(mem)) {
			return false;  // FIXME true?
		}

		if(update_casproc_is_swapped(mem)) {
			uint32_t size = *(uint32_t*)(mem + 4);
			vecoff_t off  = *(vecoff_t*)(mem + 8);
			add_free_pool(off, size);
		}

		bk->is_free_block = false;

		std::pair<leases_t::iterator, bool> ins = m_leases.insert(
				leases_t::value_type(key, lease_entry(bk, ct)) );
		if(!ins.second) { return false; }  // FIXME

		__sync_fetch_and_add(&bk->m_refcount, 1);

		return true;
	}
}


bool ostorage::remove(std::string key, ClockTime ct)
{
	char mem[16];
	*(uint32_t*)mem                = ct.time();        // FIXME endian
	*(uint32_t*)(((char*)mem) + 4) = 0;
	*(vecoff_t*)(((char*)mem) + 4) = 0;

	mp::pthread_scoped_lock lslk(m_leases_mutex);
	leases_t::iterator it = m_leases.find(key);
	if(it != m_leases.end()) {
		if(ct < it->second.clocktime) {
			// FIXME invalid clock, ranged compare, vector clock
			return false;
		}

		if(it->second.bk) {
			if(!tchdbput(m_index_map,
					key.data(), key.size(),
					mem, sizeof(mem))) {
				return false;  // FIXME exception?
			}

			it->second.bk->is_free_block = true;
		}
		it->second.clocktime = ct;

		return true;

	} else {
		if(!tchdbputproc(m_index_map,
				key.data(), key.size(),
				mem, sizeof(mem),
				update_casproc, (void*)mem)) {
			return false;  // FIXME exception?
		}

		if(update_casproc_is_ignored(mem)) {
			return false;  // FIXME true?
		}

		if(update_casproc_is_swapped(mem)) {
			uint32_t size = *(uint32_t*)(mem + 4);
			vecoff_t off  = *(vecoff_t*)(mem + 8);
			add_free_pool(off, size);
		}

		std::pair<leases_t::iterator, bool> ins = m_leases.insert(
				leases_t::value_type(key, lease_entry(NULL, ct)) );
		if(!ins.second) { return false; }  // FIXME

		return true;
	}
}


}  // namespace kastor

