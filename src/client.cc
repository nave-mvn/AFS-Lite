#include <iostream>
#include <memory>
#include <string>
#include <time.h>
#include <algorithm>

#include <grpc++/grpc++.h>

#include "../proto/AFSInterface.grpc.pb.h"

#include "util.h"

using std::cout;
using std::endl;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using RpcPackage::StringMessage;
using RpcPackage::RpcService;
using RpcPackage::DirMessage;
using RpcPackage::DirEntry;
using RpcPackage::StatStruct;

using namespace std;

class Client {
	public:
		Client(std::shared_ptr<Channel> channel): stub_(RpcService::NewStub(channel)) {}

		std::string test() {
			StringMessage send_path;
			DirMessage reply;
			string path = "";
			log("path: " + path);
			ClientContext context;
			Status status;
			send_path.set_msg(string(path));
			status = stub_->readdirectory(&context, send_path, &reply);
			// Act upon its status.
			if (status.ok()) {
				return "RPC passed";
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
	std::string reply = client.test();
	std::cout<<"reply: "<<reply<<std::endl;
	return 0;
}
