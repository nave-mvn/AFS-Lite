#include <iostream>
#include <string>
#include <time.h>

#include <grpc++/grpc++.h>

#include "../proto/SimpleInterface.grpc.pb.h"

#include "util.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using RpcPackage::StringMessage;
using RpcPackage::RpcService;

class SimpleServiceImpl final: public RpcService::Service{
	Status rpc_message(ServerContext* context, const StringMessage* recv_msg, StringMessage* reply_msg) override {
		//int recv_int = recv_msg->msg();
		std::string reply = std::string("ACK:" + recv_msg->msg());
		reply_msg->set_msg(reply);
		return Status::OK;
	}
};

void RunServer() {
	std::string server_address("0.0.0.0:50051");
	SimpleServiceImpl service;

	ServerBuilder builder;
	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(&service);
	// Finally assemble the server.
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Server listening on " << server_address << std::endl;

	// Wait for the server to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	server->Wait();
}

int main(int argc, char** argv) {
	RunServer();
	return 0;
}
