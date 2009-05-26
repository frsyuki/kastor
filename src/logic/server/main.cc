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
#include "server/framework.h"
#include <ccf/scoped_listen.h>
#include <ccf/service.h>
#include <stdio.h>
#include <stdlib.h>

void usage(const char* prog)
{
	printf("usage: %s <storage> [port=3000]\n", prog);
	exit(1);
}

int main(int argc, char* argv[])
{
	if(argc <= 1 || argc > 3) { usage(argv[0]); }
	const char* path = argv[1];

	if(path[0] == '-') { usage(argv[0]); }

	unsigned short port = 3000;
	
	if(argc == 3) {
		port = atoi(argv[2]);
		if(port == 0) { usage(argv[0]); }
	}

	mkdir(path, 0777);

	using namespace kastor;

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(port);

	ccf::service::init();

	ccf::scoped_listen lsock(addr);
	ostorage storage(path);
	framework::init(storage, lsock.sock());

	ccf::service::start(3, 3);
	ccf::service::join();
}

