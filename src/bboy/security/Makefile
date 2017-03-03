
CXXFLAGS += -I$(SRC_DIR)
CXXFLAGS += -std=c++11 -Wall -Werror -Wno-sign-compare -Wno-deprecated -g -c -o

CXX=g++

CPP_SOURCES :=  \
	token.pb.cc \
	token_signing_key.cc \
	token_verifier.cc \
	token_signer.cc \
	openssl_util.cc \
	crypto.cc \
	cert.cc \
	init.cc \
	x509_check_host.cc \
	ca/cert_management.cc \
	tls_socket.cc \
	tls_handshake.cc \
	tls_context.cc \

CPP_OBJECTS := $(CPP_SOURCES:.cc=.o)

STATIC_LIB=libsecurity.a

LIBS += $(STATIC_LIB)

all: $(CPP_OBJECTS) $(LIBS)

$(STATIC_LIB): $(CPP_OBJECTS)
	@echo "  [LINK] $@"
	@ar rs $@ $(CPP_OBJECTS)

.PRECIOUS:  token.pb.cc token.pb.cc

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
