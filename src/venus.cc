/*
 * This is the implementation of the AFS client, Venus, which runs on the client workstation.
 * Venus is implemented on top of the FUSE library and runs as a user-level process.
 * Venus is responsible for 
 - Serving file system requests through contacting Vice
 - Maintaining a cached copy of the frequently used files in local drive
 - Ensuring the cached copies are consistent with the original files on the Vice server at times of read / write
 */

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <fuse.h>
#include <errno.h>
#include <fcntl.h>

#include <iostream>
#include <string>

#include <grpc++/grpc++.h>

#include "../proto/AFSInterface.grpc.pb.h"
#include "util.h"

using std::cout;
using std::endl;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using RpcPackage::DirMessage;
using RpcPackage::DirEntry;
using RpcPackage::StringMessage;
using RpcPackage::StatStruct;
using RpcPackage::RpcService;

static const char *venus_str = "Hello World!\n";
static const char *venus_path = "/hello";

// The stub holds the RPC connection. In global scope.
std::unique_ptr<RpcService::Stub> stub_;

static int venus_getattr(const char *path, struct stat *stbuf)
{
	StringMessage send_path;
	StatStruct reply;
	log("path: %s \n", path);
	memset(stbuf, 0, sizeof(struct stat));
	int res = 0;
	if(strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR;
		stbuf->st_nlink = 2;
		return res;
	}
	ClientContext context;
	Status status;
	send_path.set_msg(string(path));
	status = stub_->stat_get_attr(&context, send_path, &reply);
	if(status.ok()){
		stbuf->st_dev = reply.device_id();
		stbuf->st_ino = reply.file_number();
		stbuf->st_mode = reply.file_mode();
		stbuf->st_nlink = reply.hard_links();
		stbuf->st_uid = reply.user_id();
		stbuf->st_gid = reply.group_id();
		stbuf->st_size = reply.file_size();
		stbuf->st_atime = reply.time_access();
		stbuf->st_mtime = reply.time_mod();
		stbuf->st_ctime = reply.time_chng();
		log("%i", stbuf->st_ino);
	}
	else{
		res = -ENOENT;
	}
	return res;
}

static int venus_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi)
{
	int res = 0;
	StringMessage send_path;
	DirMessage reply;
	log("path: %s \n", path);
	ClientContext context;
	Status status;
	send_path.set_msg(string(path));
	status = stub_->readdirectory(&context, send_path, &reply);
	if(status.ok()){
		int size = reply.dir_size();
		if(reply.success() == false){
			log("Returning false");
			return -ENOENT;
		}
		log("Size: %i", size);
		for(int i=0; i<size; ++i){
			DirEntry entry = reply.dir(i);
			struct stat st;
                	memset(&st, 0, sizeof(st));
                	st.st_ino = entry.file_number();
                	st.st_mode = entry.file_mode();
			log("Name:" + entry.name());
                	if (filler(buf, entry.name().c_str(), &st, 0)){
				break;
			}
		}
	}
	else{
		res = -ENOENT;
	}
	return res;
}

static int venus_open(const char *path, struct fuse_file_info *fi)
{
	if (strcmp(path, venus_path) != 0)
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int venus_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	size_t len;
	(void) fi;
	if(strcmp(path, venus_path) != 0)
		return -ENOENT;

	len = strlen(venus_str);
	if (offset < len) {
		if (offset + size > len)
			size = len - offset;
		memcpy(buf, venus_str + offset, size);
	} else
		size = 0;

	return size;
}

int main(int argc, char *argv[])
{
	open_err_log();
	static struct fuse_operations venus_oper;
	venus_oper.getattr = venus_getattr;
	venus_oper.readdir = venus_readdir;
	venus_oper.open = venus_open;
	venus_oper.read = venus_read;
	std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("192.168.1.126:50051", grpc::InsecureCredentials());
	stub_ = RpcService::NewStub(channel);
	return fuse_main(argc, argv, &venus_oper, NULL);
}
