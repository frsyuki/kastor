//
// mp::sparse_array
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

#ifndef MP_SPARSE_ARRAY_IMPL_H__
#define MP_SPARSE_ARRAY_IMPL_H__

#include <stdexcept>

namespace mp {


template <typename T>
sparse_array<T>::sparse_array()
{
	extend_array();
}

template <typename T>
sparse_array<T>::~sparse_array()
{
	for(typename base_array_t::iterator it(base_array.begin()), it_end(base_array.end());
			it != it_end;
			++it ) {
		for(item_t *p(*it), *p_end(p+EXTEND_ARRAY_SIZE); p != p_end; ++p) {
			if(p->enable) {
				reinterpret_cast<T*>(p->data)->~T();
			}
		}
		std::free(*it);
	}
}

template <typename T>
T& sparse_array<T>::set(size_type index)
try {
	return *(new (set_impl(index)) T());
} catch (...) {
	revert(index);
	throw;
}
MP_ARGS_BEGIN
template <typename T>
template <MP_ARGS_TEMPLATE>
T& sparse_array<T>::set(size_type index, MP_ARGS_PARAMS)
try {
	return *(new (set_impl(index)) T(MP_ARGS_FUNC));
} catch (...) {
	revert(index);
	throw;
}
MP_ARGS_END

template <typename T>
void sparse_array<T>::reset(size_type index)
{
	item_t& item(item_of(index));
	item.enable = false;
	reinterpret_cast<T*>(item.data)->~T();
}

template <typename T>
T& sparse_array<T>::data(size_type index)
{
	return *reinterpret_cast<T*>(item_of(index).data);
}

template <typename T>
const T& sparse_array<T>::data(size_type index) const
{
	return *reinterpret_cast<const T*>(item_of(index).data);
}

template <typename T>
T* sparse_array<T>::get(size_type index)
{
	if( base_array.size() * EXTEND_ARRAY_SIZE > index ) {
		item_t& item(item_of(index));
		if( item.enable ) {
			return reinterpret_cast<T*>(item.data);
		}
	}
	return NULL;
}

template <typename T>
const T* sparse_array<T>::get(size_type index) const
{
	if( base_array.size() * EXTEND_ARRAY_SIZE > index ) {
		item_t& item(item_of(index));
		if( item.enable ) {
			return reinterpret_cast<const T*>(item.data);
		}
	}
	return NULL;
}

template <typename T>
bool sparse_array<T>::test(size_type index) const
{
	return base_array.size() * EXTEND_ARRAY_SIZE > index &&
		item_of(index).enable;
}

template <typename T>
typename sparse_array<T>::size_type sparse_array<T>::capacity() const
{
	return base_array.size() * EXTEND_ARRAY_SIZE;
}

template <typename T>
void* sparse_array<T>::set_impl(size_type index)
{
	while( base_array.size() <= index / EXTEND_ARRAY_SIZE ) {
		extend_array();
	}
	item_t& item(item_of(index));
	if( item.enable ) {
		reinterpret_cast<T*>(item.data)->~T();
	} else {
		item.enable = true;
	}
	return item.data;
}

template <typename T>
void sparse_array<T>::revert(size_type index)
{
	item_of(index).enable = false;
}

template <typename T>
void sparse_array<T>::extend_array()
{
	item_t* ex = (item_t*)std::calloc(EXTEND_ARRAY_SIZE, sizeof(item_t));
	if(!ex) { throw std::bad_alloc(); }
	base_array.push_back(ex);
}

template <typename T>
typename sparse_array<T>::item_t& sparse_array<T>::item_of(size_type index)
{
	return base_array[index / EXTEND_ARRAY_SIZE][index % EXTEND_ARRAY_SIZE];
}

template <typename T>
const typename sparse_array<T>::item_t& sparse_array<T>::item_of(size_type index) const
{
	return base_array[index / EXTEND_ARRAY_SIZE][index % EXTEND_ARRAY_SIZE];
}


}  // namespace mp

#endif /* mp/sparse_array.h */

