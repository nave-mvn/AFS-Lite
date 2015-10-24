CXX = g++
CPPFLAGS += -I/usr/local/include -pthread
CXXFLAGS += -std=c++11
LDFLAGS += -L/usr/local/lib -lgrpc++_unsecure -lgrpc -lgpr -lprotobuf -lpthread -ldl
PROTOC = protoc
GRPC_CPP_PLUGIN = grpc_cpp_plugin
GRPC_CPP_PLUGIN_PATH ?= `which $(GRPC_CPP_PLUGIN)`

PROTOS_PATH = proto
SRC_PATH = src
OUT_PATH = bin

RPC_INTERFACE = SimpleInterface

vpath %.proto $(PROTOS_PATH)

all: clean $(RPC_INTERFACE).pb.cc $(RPC_INTERFACE).grpc.pb.cc rpc_client rpc_server

# TODO - the RPC client and server should be a shared library and not have main functions as they have now. The rpc client should expose a interface to the venus and the rpc server should provide callbacks for vice to register to.
rpc_client: $(PROTOS_PATH)/$(RPC_INTERFACE).pb.o $(PROTOS_PATH)/$(RPC_INTERFACE).grpc.pb.o $(SRC_PATH)/RPCClient.o
	$(CXX) -O $^ $(LDFLAGS) -o $(OUT_PATH)/$@

rpc_server: $(PROTOS_PATH)/$(RPC_INTERFACE).pb.o $(PROTOS_PATH)/$(RPC_INTERFACE).grpc.pb.o $(SRC_PATH)/RPCServer.o
	$(CXX) -O $^ $(LDFLAGS) -o $(OUT_PATH)/$@

%.grpc.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --grpc_out=$(PROTOS_PATH) --plugin=protoc-gen-grpc=$(GRPC_CPP_PLUGIN_PATH) $<

%.pb.cc: %.proto
	$(PROTOC) -I $(PROTOS_PATH) --cpp_out=$(PROTOS_PATH) $<

clean:
	rm -f $(SRC_PATH)/*.o $(PROTOS_PATH)/*.pb.* $(OUT_PATH)/*
