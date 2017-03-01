
include build_config.mk
export

SRC_PREFIX=${CURDIR}/bboy

all:
	make -C ${SRC_PREFIX}/gbase
	make -C ${SRC_PREFIX}/base
	make -C ${SRC_PREFIX}/tests/base

#	make -C ${SRC_PREFIX}/app/zk
#	make -C ${SRC_PREFIX}/common
#	make -C ${SRC_PREFIX}/server
#	make -C ${SRC_PREFIX}/master
#	make -C ${SRC_PREFIX}/worker_server
#	make -C ${SRC_PREFIX}/tests/master
#	make -C ${SRC_PREFIX}/tests/rpc
#	make -C ${SRC_PREFIX}/tests/util

clean:
	make -C ${SRC_PREFIX}/gbase clean
	make -C ${SRC_PREFIX}/base clean
	make -C ${SRC_PREFIX}/tests/base clean

#	make -C ${SRC_PREFIX}/common clean
#	make -C ${SRC_PREFIX}/server clean
#	make -C ${SRC_PREFIX}/master clean
#	make -C ${SRC_PREFIX}/worker_server clean
#	make -C ${SRC_PREFIX}/tests/rpc clean
#	make -C ${SRC_PREFIX}/tests/util clean

