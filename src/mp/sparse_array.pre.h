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

#ifndef MP_SPARSE_ARRAY_H__
#define MP_SPARSE_ARRAY_H__

#include <vector>
#include <cstdlib>

namespace mp {


template <typename T>
class sparse_array {
public:
	typedef size_t size_type;

	sparse_array();
	~sparse_array();

	//! Set instance of T into index.
	inline T& set(size_type index);
MP_ARGS_BEGIN
	template <MP_ARGS_TEMPLATE>
	inline T& set(size_type index, MP_ARGS_PARAMS);
MP_ARGS_END

	//! Reset index.
	inline void reset(size_type index);

	//! Get the data of index.
	inline T& data(size_type index);
	inline const T& data(size_type index) const;

	//! Get the data of index.
	inline T* get(size_type index);
	inline const T* get(size_type index) const;

	//! Return true if index is set otherwise false.
	inline bool test(size_type index) const;

	inline size_type capacity() const;

private:
	static const size_t EXTEND_ARRAY_SIZE = 64;

	struct item_t {
		bool enable;
		char data[sizeof(T)];
	};

	typedef std::vector<item_t*> base_array_t;
	base_array_t base_array;

private:
	inline void* set_impl(size_type index);
	inline void revert(size_type index);
	inline void extend_array();
	inline item_t& item_of(size_type index);
	inline const item_t& item_of(size_type index) const;
};


}  // namespace mp

#include "mp/sparse_array_impl.h"

#endif /* mp/sparse_array.h */

