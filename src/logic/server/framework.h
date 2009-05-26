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
#ifndef FRAMEWORK_H__
#define FRAMEWORK_H__

#include "server/ostorage.h"
#include <memory>

namespace kastor {


class framework {
public:
	static void init(ostorage& storage, int lsock);

	framework(ostorage& storage, int lsock);
	~framework();

	ostorage& storage() { return m_storage; }

private:
	ostorage& m_storage;

private:
	framework();
	framework(const framework&);
};


extern std::auto_ptr<framework> net;


}  // namespace kastor

#endif /* framework.h */

