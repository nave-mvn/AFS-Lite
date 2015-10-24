#include <iostream>
#include <memory>
#include <string>
#include <time.h>
#include <algorithm>

#include <grpc++/grpc++.h>

#include "../proto/SimpleInterface.grpc.pb.h"

#include "util.h"

#define ITERATIONS 10

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using RpcPackage::StringMessage;
using RpcPackage::RpcService;

class Client {
	public:
		Client(std::shared_ptr<Channel> channel): stub_(RpcService::NewStub(channel)) {}

		std::string test_round_trip() {

			StringMessage send_message;
			// Container for the data we expect from the server.
			StringMessage reply_message;
			// Context for the client. It could be used to convey extra information to
			// the server and/or tweak certain RPC behaviors.
			Status status;

			// The actual RPC.
			for(int i=0; i<ITERATIONS; ++i){
				ClientContext context;
				timespec before_tspec, after_tspec;
				std::string r_string = random_string(10);
				send_message.set_msg(r_string);
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &before_tspec);
				status = stub_->rpc_message(&context, send_message, &reply_message);
				clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &after_tspec);
				long time_diff = diff(before_tspec, after_tspec).tv_nsec;
				std::cout<<"Time to round trip: "<<time_diff<<std::endl;		
			}

			// Act upon its status.
			if (status.ok()) {
				return reply_message.msg();
			} else {
				return "RPC failed";
			}
		}

	private:
		std::unique_ptr<RpcService::Stub> stub_;
};


int main(int argc, char** argv) {
	//Client client(grpc::CreateChannel("localhost:50051", grpc::InsecureCredentials()));
	Client client(grpc::CreateChannel("192.168.1.126:50051", grpc::InsecureCredentials()));
	std::string reply = client.test_round_trip();
	std::cout<<"reply: "<<reply<<std::endl;
	return 0;
}
