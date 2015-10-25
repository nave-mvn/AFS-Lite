/**
 * The vice server
 */

#include <iostream>
#include <string>
#include <time.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <dirent.h>

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
using RpcPackage::DirMessage;
using RpcPackage::DirEntry;
using RpcPackage::RpcService;

std::string* root_dir;

class Vice final: public RpcService::Service{
	Status stat_get_attr(ServerContext* context, const StringMessage* recv_msg, StatStruct* reply_struct) override {
		struct stat stbuf;
		memset(&stbuf, 0, sizeof(struct stat));
		string full_path = *root_dir + recv_msg->msg();
		log(recv_msg->msg());
		log(full_path);
		int res = lstat(full_path.c_str(), &stbuf);
		reply_struct->set_file_mode(S_IFREG | 0444);
		reply_struct->set_hard_links(1);
		reply_struct->set_file_size(4096);
		return Status::OK;
	}
	/*
	   Status readdirectory(ServerContext* context, const StringMessage* recv_msg, DirMessage* reply_msg) override {
	   string full_path = *root_dir + recv_msg->msg();
	   log("read dir called " + full_path);
	   DIR *dp;
	   dp = opendir(full_path.c_str());
	   log("opened dir \n");
	   if (dp == NULL){
	   reply_msg->set_success(false);
	   return Status::OK;
	   }
	   log("not returning false \n");
	   reply_msg->set_success(true);
	   while (true) {
	   struct dirent *de;
	   if((de = readdir(dp)) == NULL){
	   break;
	   }
	   log("reading struct 1 \n");
	   struct stat st;
	   memset(&st, 0, sizeof(st));
	   DirEntry dirEntry;
	   StatStruct stat;
	   log("reading struct 2 \n");
	   stat.set_file_number(de->d_ino);
	   stat.set_file_mode(de->d_type << 12);
	   dirEntry.set_allocated_stat(&stat);
	   log("reading struct 3 \n");
	   DirEntry* ent = reply_msg->add_dir();
	   ent->set_name(de->d_name);
	   log("reading struct 4 -" + string(de->d_name) + "\n");
	   free(de);
	   }
	   log("about to close \n");
	   closedir(dp);
	   return Status::OK;
	   }*/

	Status readdirectory(ServerContext* context, const StringMessage* recv_msg, DirMessage* reply_msg) override {
		DIR *dirp;
    		struct dirent *dp;
		string full_path = *root_dir + recv_msg->msg();
		log("read dir called " + full_path);
		if ((dirp = opendir(full_path.c_str())) == NULL) {
			perror("couldn't open '.'");
			return Status::OK;
		}
		do {
			errno = 0;
			if ((dp = readdir(dirp)) != NULL) {
				log("found\n");
			}
		} while (dp != NULL);
		closedir(dirp);
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
	open_err_log();
	root_dir = new string(argv[1]);
	run_server();
	return 0;
}
