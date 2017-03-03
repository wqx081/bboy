
CXXFLAGS += -I$(SRC_DIR)
CXXFLAGS += -std=c++11 -Wall -Werror -Wno-sign-compare -Wno-deprecated -g -c -o

CXX=g++

CPP_SOURCES :=  \
	pb_util.pb.cc \
	errno.cc \
	malloc.cc \
	monotime.cc \
	faststring.cc \
	os-util.cc \
	slice.cc \
	status.cc \
	status_callback.cc \
	path_util.cc \
	env.cc \
	env_posix.cc \
	user.cc \
	random_util.cc \
	debug_util.cc \
	mem_tracker.cc \
	\
	\
	process/subprocess.cc \
	\
	json/jsonreader.cc \
	json/jsonwriter.cc \
	\
	net/net_util.cc \
	net/sockaddr.cc \
	net/socket.cc \
	\
	thread/thread.cc \
	thread/threadlocal.cc \
	thread/threadpool.cc \
	\
	sync/mutex.cc \
	sync/condition_variable.cc \
	sync/atomic.cc \
	sync/semaphore.cc \
	sync/rw_mutex.cc \
	sync/rw_semaphore.cc \
	sync/locks.cc \
	sync/rwc_lock.cc \
	sync/barrier.cc \
	sync/countdown_latch.cc \
	sync/once.cc \

CPP_OBJECTS := $(CPP_SOURCES:.cc=.o)

STATIC_LIB=libbase.a

LIBS += $(STATIC_LIB)

all: $(CPP_OBJECTS) $(LIBS)

$(STATIC_LIB): $(CPP_OBJECTS)
	@echo "  [LINK] $@"
	@ar rs $@ $(CPP_OBJECTS)

.PRECIOUS:  histogram.pb.cc pb_util.pb.cc

.cc.o:
	@echo "  [CXX]  $@"
	@$(CXX) $(CXXFLAGS) $@ $<

%.pb.cc: %.proto
	@echo "  [GEN]  $@"
	@protoc  --cpp_out $(SRC_DIR) --proto_path $(SRC_DIR) --proto_path /usr/local/include $(CURDIR)/$<

clean:
	@rm -fr $(CPP_OBJECTS)
	@rm -fr $(LIBS)
	@rm -fr *.pb.h *.pb.cc
