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
#include "ccf/service.h"
#include <mp/pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

namespace ccf {


/*
std::auto_ptr<mp::wavy::core> event;
std::auto_ptr<mp::wavy::net>  net;
*/


namespace service {


static std::auto_ptr<mp::pthread_signal> s_signal_thread;

static void signal_handler(void*, int signo)
{
	stop();
}

void init()
{
	//event.reset(new mp::wavy::core());
	//net.reset(new mp::wavy::net());
	wavy::init(0, 0);

	sigset_t ss;
	sigemptyset(&ss);
	sigaddset(&ss, SIGHUP);
	sigaddset(&ss, SIGINT);
	sigaddset(&ss, SIGTERM);

	s_signal_thread.reset( new mp::pthread_signal(ss,
				&signal_handler, NULL) );
}

void daemonize(const char* pidfile, const char* stdio)
{
	pid_t pid;
	pid = fork();
	if(pid < 0) { perror("fork"); exit(1); }
	if(pid != 0) { exit(0); }
	if(setsid() == -1) { perror("setsid"); exit(1); }
	pid = fork();
	if(pid < 0) { perror("fork"); exit(1); }
	if(pid != 0) { exit(0); }
	if(pidfile) {
		FILE* f = fopen(pidfile, "w");
		if(!f) { perror("can't open pid file"); exit(1); }
		fprintf(f, "%d", getpid());
		fclose(f);
	}
	if(stdio) {
		int r = open(stdio, O_RDONLY);
		if(r < 0) { perror(stdio); exit(1); }
		int a = open(stdio, O_APPEND);
		if(a < 0) { perror(stdio); exit(1); }
		close(0);
		close(1);
		close(2);
		if(dup2(r, 0) < 0) { perror("dup2"); exit(1); }
		if(dup2(a, 1) < 0) { perror("dup2"); exit(1); }
		if(dup2(a, 2) < 0) { perror("dup2"); exit(1); }
		close(r);
		close(a);
	}
}

void start(size_t rthreads, size_t wthreads)
{
	//event->join(rthreads);
	//net->join(wthreads);
	wavy::add_rthread(rthreads);
	wavy::add_wthread(wthreads);
}

void join()
{
	//event->join();
	//net->join();
	wavy::join();
}


void stop()
{
	//event->end();
	//net->end();
	wavy::end();
}


}  // namespace service
}  // namespace ccf

