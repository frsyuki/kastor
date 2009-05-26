export ERB
export RUBY
export RAGEL

AM_CPPFLAGS   = -I..
AM_C_CPPFLAGS = -I..

bin_PROGRAMS = kastor

kastor_SOURCES = \
		server/framework.cc \
		server/ostorage.cc \
		server/ostorage_http.cc \
		server/main.cc

kastor_LDADD = \
		../ccf/libccf.a \
		../mpsrc/libmpio.a

noinst_HEADERS = \
		server/clock.h \
		server/framework.h \
		server/http_handler.h \
		server/http_handler_impl.h \
		server/ostorage.h \
		server/ostorage_http.h \
		server/service_listener.h \
		server/types.h

# work around for duplicated file name
kastor_server_CFLAGS = $(AM_CFLAGS)
kastor_server_CXXFLAGS = $(AM_CXXFLAGS)

