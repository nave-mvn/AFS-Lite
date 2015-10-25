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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <string>
#include <time.h>
#include <algorithm>

#include <grpc++/grpc++.h>
#include "../proto/AFSInterface.grpc.pb.h"

using std::cout;
using std::endl;

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using RpcPackage::StringMessage;
using RpcPackage::RpcService;

static const char *venus_str = "Hello World!\n";
static const char *venus_path = "/hello";

std::unique_ptr<RpcService::Stub> stub_;

static int venus_getattr(const char *path, struct stat *stbuf)
{
	StringMessage send_message;
	StringMessage reply_message;
	printf("path: %s \n", path);
	ClientContext context;
	stub_->rpc_message(&context, send_message, &reply_message);
	int res = 0;
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (strcmp(path, venus_path) == 0) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = strlen(venus_str);
	} else
		res = -ENOENT;

	return res;
}

static int venus_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		off_t offset, struct fuse_file_info *fi)
{
	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	filler(buf, venus_path + 1, NULL, 0);

	return 0;
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
	static struct fuse_operations venus_oper;
	venus_oper.getattr = venus_getattr;
	venus_oper.readdir = venus_readdir;
	venus_oper.open = venus_open;
	venus_oper.read = venus_read;
	std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("192.168.1.126:50051", grpc::InsecureCredentials());
	stub_ = RpcService::NewStub(channel);
	return fuse_main(argc, argv, &venus_oper, NULL);
}
