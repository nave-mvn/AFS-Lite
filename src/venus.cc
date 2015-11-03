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
#include <unistd.h>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

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

using RpcPackage::BytesMessage;
using RpcPackage::DirMessage;
using RpcPackage::DirEntry;
using RpcPackage::StringMessage;
using RpcPackage::StatStruct;
using RpcPackage::IntMessage;
using RpcPackage::ReadMessage;
using RpcPackage::BooleanMessage;
using RpcPackage::LongMessage;
using RpcPackage::RpcService;

#define CACHE_SIZE 5

int client_id;
// hardcoded string -- have to change if hosting client on any other machine
static const char *cache_dir = "/home/naveen/.afs_cache";
static string* cache_dir_path;

// The stub holds the RPC connection. In global scope.
std::unique_ptr<RpcService::Stub> stub_;

std::map<string, string>* cached_files;
std::map<string, long>* cached_files_remote_modified;

std::map<string, long>* cached_files_local_access;//does not need to be persisted

static int get_modified_timestamp(char* path, long* timestamp){
	ClientContext context;
	Status status;
	StringMessage file_path;
	LongMessage file_mod;
	file_path.set_msg(string(path));
	status = stub_->filetime(&context, file_path, &file_mod);
	if(status.ok()){
		*timestamp = file_mod.msg();
		return 0;
	}
	else{
		return -1;
	}	
}

int read_file_into_cache(const char* path){
	StringMessage file_path;
	BytesMessage file;
	file_path.set_msg(path);
	ClientContext context;
	std::unique_ptr<ClientReader<BytesMessage> > reader(stub_->readfile(&context, file_path));
	ofstream output_file;
	string cached_file_name;
	// wow a do-while loop!
	// get a unique hash name
	log("generating unique name");
	do{
		cached_file_name = string(*cache_dir_path).append(random_string(10));
	}while(cached_files->find(cached_file_name) != cached_files->end());
	log("caching file" + cached_file_name);
	output_file.open(cached_file_name, ios::out);
	long remote_timestamp = 0;
	while (reader->Read(&file)){
		log("Received %i", file.size());
		output_file.write(file.msg().c_str(), file.size());
		remote_timestamp = file.timestamp();
	}
	Status status = reader->Finish();
	if (status.ok()) {
		log("Inital read download succeeded");
		cached_files->insert(std::pair<string, string>(string(path), cached_file_name)); 
		cached_files_remote_modified->insert(std::pair<string, long>(string(path), remote_timestamp)); 
		log("Remote timestamp is %lu", remote_timestamp);
		return 0;
	} 
	else {
		log("Inital read download failed");
		return -errno;
	}
	output_file.close();
}

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

static int venus_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	log("read file called");
	log("path: %s", path);
	std::map<string, string>::iterator cached_files_it = cached_files->find(string(path));
	if(cached_files_it == cached_files->end()){
		log("File not in cache...caching");
		int res = read_file_into_cache(path);
		if(res == -1){
			return -errno;
		}
		cached_files_it = cached_files->find(string(path)); 
		if(cached_files_it == cached_files->end()){
			log("Still cannot find file -- THIS SHOULD NOT HAPPEN");
			return -errno;
		}
	}
	log("File in cache..begin reading");
	int fd;
	int res;
	log("Cached file location: " + cached_files_it->second);
	string cached_path = cached_files_it->second;
	log("Opening.." + cached_path);
	fd = open(cached_path.c_str(), O_RDONLY);
	if (fd == -1){
		log("Could not open local file");
		return -errno;
	}
	res = pread(fd, buf, size, offset);
	if (res == -1){
		log("Could not read local file");
		return -errno;
	}
	close(fd);
	return res;
}

static int venus_open(const char *path, struct fuse_file_info *fi)
{
	//set file access time
	log("open file called");
	log("path: %s", path);
	std::map<string, string>::iterator cached_files_it = cached_files->find(string(path));
	if(cached_files_it == cached_files->end()){
		log("File not found in local cache " + string(path));
		if(read_file_into_cache(path) == -1){
			return -errno;
		}
	}
	// check if invalidate cache
	std::map<string, long>::iterator cached_files_access_it = cached_files_local_access->find(string(path));
	if(cached_files_access_it == cached_files_local_access->end()){
		cached_files_local_access->insert(std::pair<string, long>(string(path), time(NULL)));
	}
	else{
		cached_files_access_it->second = time(NULL);
	}
	return 0;
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
	cached_files_remote_modified = new std::map<string, long>();
	cached_files_local_access = new std::map<string, long>();
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
	argc = 3;
	return fuse_main(argc, argv, &venus_oper, NULL);
}
