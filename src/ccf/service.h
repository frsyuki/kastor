//
// ccf::service - Cluster Communication Framework
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
#ifndef CCF_SERVICE_H__
#define CCF_SERVICE_H__

#include "ccf/wavy.h"

namespace ccf {
namespace service {


void init();

void daemonize(const char* pidfile = NULL, const char* stdio = "/dev/null");

void start(size_t rthreads, size_t wthreads);

void join();

void stop();


}  // namespace service

/*
extern std::auto_ptr<mp::wavy::core> event;
extern std::auto_ptr<mp::wavy::net>  net;
*/


}  // namespace ccf

#endif /* ccf/service.h */

