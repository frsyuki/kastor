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
#ifndef CLOCK_H__
#define CLOCK_H__

#include <limits>
#include <stdint.h>
#include <time.h>
#include <cstdlib>

// FIXME 5 sec.
#ifndef TIME_ERROR_MARGIN
#define TIME_ERROR_MARGIN 5
#endif

namespace kastor {


class Clock {
public:
	explicit Clock(uint32_t n) : m(n) {}
	Clock() : m(0) { }  // FIXME randomize?  // FIXME invalid clock?
	~Clock() {}

public:
	uint32_t get_incr()
	{
		//return m++;
		return __sync_fetch_and_add(&m, 1);
	}

	uint32_t get() const
	{
		return m;
	}

	void update(uint32_t o)
	{
		while(true) {
			uint32_t x = m;
			if(!clock_less(x, o)) { return; }
			if(__sync_bool_compare_and_swap(&m, x, o)) {
					return;
			}
		}
		//if(clock_less(m, o)) {
		//	m = o;
		//}
	}

	bool operator< (const Clock& o) const
	{
		return clock_less(m, o.m);
	}

	static bool clock_less(uint32_t x, uint32_t y)
	{
		if((x < (((uint32_t)1)<<10) && (((uint32_t)1)<<22) < y) ||
		   (y < (((uint32_t)1)<<10) && (((uint32_t)1)<<22) < x)) {
			return x > y;
		} else {
			return x < y;
		}
	}

private:
	volatile uint32_t m;
};


class ClockTime {
public:
	ClockTime(Clock c, uint32_t t) :
		m( (((uint64_t)t) << 32) | c.get() ) {}

	explicit ClockTime(uint64_t n) : m(n) {}

	ClockTime() : m(0) { }

	~ClockTime() {}

public:
	uint64_t get() const { return m; }

	uint32_t time() const { return m & 0xffffffff; }

	Clock clock() const {
		return Clock(m&0xffffffff);
	}

	ClockTime before_sec(uint32_t sec)
	{
		return ClockTime( m - (((uint64_t)sec) << 32) );
	}

	bool operator== (const ClockTime& o) const
	{
		return m == o.m;
	}

	bool operator!= (const ClockTime& o) const
	{
		return !(*this == o);
	}

	bool operator< (const ClockTime& o) const
	{
		return clocktime_less(m, o.m);
	}

	bool operator> (const ClockTime& o) const
	{
		return clocktime_less(o.m, m);
	}

	bool operator<= (const ClockTime& o) const
	{
		return !(*this > o);
	}

	bool operator>= (const ClockTime& o) const
	{
		return !(*this < o);
	}

private:
	static bool clocktime_less(uint64_t x, uint64_t y)
	{
		uint32_t xt = x>>32;
		uint32_t yt = y>>32;
		if( std::abs((int)(xt - yt)) < TIME_ERROR_MARGIN ) {
			return Clock::clock_less(x&0xffffffff, y&0xffffffff);
		} else {
			return xt < yt;
		}
	}

private:
	volatile uint64_t m;
};


}  // namespace kastor

#endif /* clock.h */

