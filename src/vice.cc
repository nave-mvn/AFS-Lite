/**
 * The vice server
 */

#include <iostream>
#include <string>
#include <time.h>
#include <fcntl.h>

#include <grpc++/grpc++.h>

#include "../proto/AFSInterface.grpc.pb.h"

#include "util.h"

using std::cout;
using std::endl;

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using RpcPackage::StringMessage;
using RpcPackage::StatStruct;
using RpcPackage::RpcService;

class Vice final: public RpcService::Service{
	Status stat_get_attr(ServerContext* context, const StringMessage* recv_msg, StatStruct* reply_struct) override {
		std::string reply = std::string("ACK:" + recv_msg->msg());
		reply_struct->set_file_mode(S_IFREG | 0444);
		reply_struct->set_hard_links(1);
		reply_struct->set_file_size(4096);
		return Status::OK;
	}
};

void run_server() {
	std::string server_address("0.0.0.0:50051");
	Vice service;

	ServerBuilder builder;
	// Listen on the given address without any authentication mechanism.
	builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
	// Register "service" as the instance through which we'll communicate with
	// clients. In this case it corresponds to an *synchronous* service.
	builder.RegisterService(&service);
	// Finally assemble the server.
	std::unique_ptr<Server> server(builder.BuildAndStart());
	std::cout << "Vice Server listening on " << server_address << std::endl;

	// Wait for the server to shutdown. Note that some other thread must be
	// responsible for shutting down the server for this call to ever return.
	server->Wait();
}

int main(int argc, char** argv) {
	cout<<string(argv[1])<<endl;
	run_server();
	return 0;
}
