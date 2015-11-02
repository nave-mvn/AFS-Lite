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

#include <stdlib.h>
#include <stdio.h>

#include <fstream>
#include <iostream>
#include <string>

#include <grpc++/grpc++.h>

#include "../proto/AFSInterface.grpc.pb.h"
#include "util.h"

using std::cout;
using std::endl;
using std::string;

using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using RpcPackage::ByteBuffer;
using RpcPackage::DirMessage;
using RpcPackage::DirEntry;
using RpcPackage::StringMessage;
using RpcPackage::StatStruct;
using RpcPackage::IntMessage;
using RpcPackage::ReadMessage;
using RpcPackage::BooleanMessage;
using RpcPackage::RpcService;

#define CACHE_SIZE 5

int client_id;
// hardcoded string -- have to change if hosting client on any other machine
static const char *cache_dir = "/home/naveen/.afs_cache";
static string* cache_dir_path;

// The stub holds the RPC connection. In global scope.
std::unique_ptr<RpcService::Stub> stub_;

std::map<string, string>* cached_files;
std::map<string, long>* cached_files_timestamp;

void make_cache_dir(){
	struct stat sb;
	if(stat(cache_dir_path->c_str(), &sb) == 0 && S_ISDIR(sb.st_mode)){
		log("Cache directory present");
	}
	else{
		if(mkdir(cache_dir_path->c_str(), 0777) == -1){
			log("Failed to create cache directory");
		}
		else{
			log("Cache dir created");
		}
	}	
}

static int venus_chown(const char *path, uid_t uid, gid_t gid)
{
	log("chown called");
	return 0;
}

static int venus_access(const char *path, int mask)
{
	// only one user, access to all files, return 0 always
	return 0;
}

static int venus_getxattr(const char *path, const char *name, char *value, size_t size)
{
	log("getxattr called");
	return 0;
}

static int venus_opendir(const char *path, struct fuse_file_info *fi)
{
	log("opendir called");
	return 0;
}

static int venus_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	/* Just a stub.  This method is optional and can safely be left
	   unimplemented */
	log("fsync called");
	return 0;
}

static int venus_statfs(const char *path, struct statvfs *stbuf)
{
	log("statfs called");
	return 0;
}

static int venus_read(const char *path, char *buf, size_t size, off_t offset,
		struct fuse_file_info *fi)
{
	log("read file called");
	int res = 0;
	/*
	   ReadMessageReq send_msg;
	   ByteBuffer reply;
	   log("path: %s", path);
	   ClientContext context;
	   Status status;
	   send_msg.set_path(string(path));
	   send_msg.set_size(size);
	   send_msg.set_offset(offset);
	   status = stub_->readfile(&context, send_msg, &reply);
	   if(status.ok()){
	   log("Read %i chars" + reply->byte_size());
	   res = reply->byte_size();
	   }
	   else{
	   log("Read error");
	   res = -errno;
	   }
	   log("----------------------");
	 */
	return res;
}

static int venus_open(const char *path, struct fuse_file_info *fi)
{
	log("open file called");
	log("path: %s", path);
	std::map<string, string>::iterator cached_files_it = cached_files->find(string(path));
	if(cached_files_it != cached_files->end()){
		log("File found in local cache " + string(path));
		// do local read
		return 0;
	}
	ReadMessage read_msg;
	StringMessage file;
	read_msg.set_path(string(path));
	read_msg.set_clientid(client_id);
	ClientContext context;
	std::unique_ptr<ClientReader<StringMessage> > reader(stub_->readfile(&context, read_msg));
	ofstream output_file;
	string cached_file_name = string(*cache_dir_path).append(random_string(10));
	log("file name " + cached_file_name);
	output_file.open(cached_file_name, ios::out|ios::binary);
	while (reader->Read(&file)) {
		output_file.write(file.msg().c_str(), sizeof(char));
	}
	Status status = reader->Finish();
	if (status.ok()) {
		log("Inital read download succeeded");
	} 
	else {
		log("Inital read download failed");
	}
	output_file.close();

	/*
	   static int venus_open(const char *path, struct fuse_file_info *fi)
	   {
	   log("open file called");
	   StringMessage send_path;
	   IntMessage reply;
	   log("path: %s", path);
	   int res = 0;
	   ClientContext context;
	   Status status;
	   send_path.set_msg(string(path));
	   status = stub_->openfile(&context, send_path, &reply);
	   if(status.ok()){
	   log("Returning open status");
	   int fh = reply.msg(); 
	   if(fh != -errno){
	   log("Returning file handle as %i", fh);
	   fi->fh = fh;
	   }
	   else{
	   log("Error opening file");
	   res = -errno;
	   }
	   }
	   else{
	   log("Fuse connection error in open");
	   res = -errno;
	   }
	   log("----------------------");
	   return res;
	   }
	 */
}

static int venus_getattr(const char *path, struct stat *stbuf)
{
	log("getattr called");
	StringMessage send_path;
	StatStruct reply;
	log("path: %s", path);
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
	}
	else{
		res = -ENOENT;
	}
	log("----------------------");
	return res;
}

static int venus_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	log("readdir called");
	int res = 0;
	StringMessage send_path;
	DirMessage reply;
	log("path: %s", path);
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
		for(int i=0; i<size; ++i){
			DirEntry entry = reply.dir(i);
			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = entry.file_number();
			st.st_mode = entry.file_mode();
			log(entry.name());
			if (filler(buf, entry.name().c_str(), &st, 0)){
				break;
			}
		}
	}
	else{
		res = -ENOENT;
	}
	log("----------------------");
	return res;
}

int main(int argc, char *argv[])
{
	client_id = atoi(argv[3]);
	cache_dir_path = new string(string(cache_dir).append(int_to_string(client_id)).append("/"));
	open_err_log();
	make_cache_dir();
	cached_files = new std::map<string, string>();
	cached_files_timestamp = new std::map<string, long>();
	static struct fuse_operations venus_oper;
	venus_oper.getattr = venus_getattr;
	venus_oper.readdir = venus_readdir;
	venus_oper.open = venus_open;
	venus_oper.read = venus_read;
	venus_oper.chown = venus_chown;
	venus_oper.access = venus_access;
	venus_oper.getxattr = venus_getxattr;
	venus_oper.fsync = venus_fsync;
	venus_oper.statfs = venus_statfs;
	std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("192.168.1.126:50051", grpc::InsecureCredentials());
	stub_ = RpcService::NewStub(channel);
	return fuse_main(argc, argv, &venus_oper, NULL);
}
