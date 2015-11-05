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

#define DEBUG_EXIT 2
#define DEBUG_BYTES_SIZE 5
//#define DEBUG true
#define DEBUG false
#define CACHE_SIZE 5

#define BUF_SIZE 4 * 1024

// hardcoded string -- have to change if hosting client on any other machine
static const char *cache_dir = "/home/naveen/.afs_cache";
static string* cache_dir_path;
static string SEPARATOR = "::";
static string DELETOR = "!";
// The stub holds the RPC connection. In global scope.
std::unique_ptr<RpcService::Stub> stub_;

std::map<string, string>* cached_files;
std::map<string, long>* cached_files_remote_modified;

bool flush_file = false;
bool write_in_progress = false;
bool is_crash = false;
static int get_remote_file_attr(const char* path, struct stat *stbuf){
	ClientContext context;
	StringMessage send_path;
	StatStruct reply;
	Status status;
	send_path.set_msg(string(path));
	status = stub_->stat_get_attr(&context, send_path, &reply);
	if(status.ok()){
		stbuf->st_dev = reply.device_id();
		stbuf->st_ino = reply.file_number();
		stbuf->st_mode = reply.file_mode();
		stbuf->st_nlink = reply.hard_links();
		stbuf->st_uid = 1000;
		stbuf->st_gid = 1000;
		stbuf->st_size = reply.file_size();
		stbuf->st_atime = reply.time_access();
		stbuf->st_mtime = reply.time_mod();
		stbuf->st_ctime = reply.time_chng();
		return 0;
	}
	else{
		log("Retruning Error");
		return -1;
	}
}

static int invalidate_local_cache(const char* path){
	log("invalidating file %s", path);
	std::map<string, string>::iterator cached_files_it = cached_files->find(string(path));
	if(cached_files_it == cached_files->end()){
		log("file not in cache map!");
		return -1;
	}
	string path_to_remove = cached_files_it->second;
	int ret = remove(path_to_remove.c_str());
	if(ret == 0) 
	{
		log("file deleted successfully");
		cached_files->erase(cached_files_it);
		record_cache(DELETOR + string(path));
		std::map<string, long>::iterator cached_files_mod_it = cached_files_remote_modified->find(string(path));
		if(cached_files_mod_it != cached_files_remote_modified->end()){
			cached_files_remote_modified->erase(cached_files_mod_it);
			record_mod_time(DELETOR + string(path));
		}
		//need to log removal
		return 0;
	}
	else 
	{
		log("file remove unsuccessfull %s", path);
		return -1;
	}
}

static int get_modified_timestamp(const char* path, long* timestamp){
	log("Fetching modified timestamp");
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
	bool found_unique_name = false;
	while(!found_unique_name){//wow this is inefficient!!
		cached_file_name = string(*cache_dir_path).append(random_string(10));
		found_unique_name = true;
		for (std::map<string, string>::iterator it=cached_files->begin(); it!=cached_files->end(); ++it){
    			if(it->second.compare(cached_file_name) == 0){//equal
				found_unique_name = false;
				break;
			}
		}
	}
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
		record_cache(string(path) + SEPARATOR + cached_file_name);
		cached_files_remote_modified->insert(std::pair<string, long>(string(path), remote_timestamp)); 
		record_mod_time(string(path) + SEPARATOR + any_to_string(remote_timestamp));
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

static int venus_release(const char *path, struct fuse_file_info *fi)
{
	log("release called");
	return 0;
}

static int venus_unlink(const char *path)
{
	log("unlink dir %s called", path);
	StringMessage file_path;
	BooleanMessage deletefile;
	file_path.set_msg(string(path));
	ClientContext context;
	Status status = stub_->unlinkfile(&context, file_path, &deletefile);
	log("----------------------");
	if (status.ok()){
		if(deletefile.msg() == true){
			log("no other hard links...deleting local cache");
			invalidate_local_cache(path);
		}
		return 0;
	}
	else{
		return -errno;
	}
}

static int venus_rmdir(const char* path){
	log("remove dir %s called", path);
	StringMessage dir_path;
	BooleanMessage dummy;
	dir_path.set_msg(string(path));
	ClientContext context;
	Status status = stub_->removedir(&context, dir_path, &dummy);
	log("----------------------");
	if (status.ok()){
		return 0;
	}
	else{
		return -errno;
	}
}

static int venus_mkdir(const char* path, mode_t mode){
	log("mkdir %s called", path);
	StringMessage dir_path;
	BooleanMessage dummy;
	dir_path.set_msg(string(path));
	ClientContext context;
	Status status = stub_->makedir(&context, dir_path, &dummy);
	log("----------------------");
	if (status.ok()){
		return 0;
	}
	else{
		return -errno;
	}
}

static int venus_flush(const char *path, struct fuse_file_info *fi)
{
	if(!flush_file){
		return 0;
	}
	else{
		flush_file = false;
		write_in_progress = false;
	}
	
	string temp_file_path = *cache_dir_path + "temp_file";
	if(is_crash){
		is_crash = false;
		int ret = remove(temp_file_path.c_str());
		return ret;
	}
	
	log("flushing to local disk");
	int fd = open(temp_file_path.c_str(), O_WRONLY);
	if (fd == -1){
		log("could not open file for fsync");
		return -errno;
	}
	//fysnc the changes to the copy to disk
	fsync(fd);
	close(fd);
	//rename the copy - atomic operation
	std::map<string, string>::iterator cached_files_it = cached_files->find(string(path));
	string cached_file_path = cached_files_it->second;
	int result = rename(temp_file_path.c_str(), cached_file_path.c_str());
	if(result != 0){
		return -errno;
	}	
	
	log("flushing to server called");
	// open file to read
	FILE *pFile = fopen(cached_file_path.c_str(), "rb");
	log("Writing from file path: %s", cached_file_path.c_str());
	if (pFile == NULL) {
		log("Error in opening file to flush"); 
		return -1;
	}
	BytesMessage file_path;
	LongMessage timestampMsg;
	ClientContext context;
	std::unique_ptr<ClientWriter<BytesMessage> > writer(stub_->writefile(&context, &timestampMsg));
	file_path.set_msg(string(path));
	log("Writing to remote file path: %s", path);
	writer->Write(file_path);
	char buffer[BUF_SIZE];
	for(;;){
		size_t n = fread(buffer, 1, BUF_SIZE, pFile);
		BytesMessage data;
		log("Read: %i", n);
		data.set_msg(buffer, n);
		data.set_size(n);
		writer->Write(data);
    		if (n < BUF_SIZE) { 
			break; 
		}
	}
	log("Closing read file"); 
	fclose(pFile);
	writer->WritesDone();
	Status status = writer->Finish();
	if (status.ok()) {
		long timestamp = timestampMsg.msg();
		std::map<string, long>::iterator cached_files_mod_it = cached_files_remote_modified->find(string(path));
		if(cached_files_mod_it != cached_files_remote_modified->end()){
			cached_files_mod_it->second = timestamp;
			record_mod_time(string(path) + SEPARATOR + any_to_string(timestamp));
		}
	}
	else{
		return -errno;
	}
	return 0;
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

/*
   static int venus_fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
   {
   log("fsync called");
   return 0;
   }
 */

static int venus_statfs(const char *path, struct statvfs *stbuf)
{
	log("statfs called");
	return 0;
}

static int venus_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	log("write file called on %s, size: %i, offset:%i", path, size, offset);
	log("flags %i, flush: %i, writepage:%i", fi->flags, fi->flush, fi->writepage);
	if(1807<offset<1810){
		is_crash = true;
	}
	int fd;
	int res;
	std::map<string, string>::iterator cached_files_it = cached_files->find(string(path));
	if(cached_files_it == cached_files->end()){
		log("could not find file in local cache!");
		return -errno;
	}
	log("Found file in local cache!");
	string temp_file_path = *cache_dir_path + "temp_file";
	string cached_file_path = cached_files_it->second;
	// make a copy of original file
	if(!write_in_progress){
		log("first write...creating temp file");
		copy_file(cached_file_path.c_str(), temp_file_path.c_str());
		write_in_progress = true;
	}
	// write to the copy
	fd = open(temp_file_path.c_str(), O_WRONLY);
	if (fd == -1){
		log("could not open file for write");
		return -errno;
	}
	log("Attempting pwrite");
	res = pwrite(fd, buf, size, offset);
	if (res == -1){
		log("write failed");
		return -errno;
	}
	close(fd);
	flush_file = true;
	return res;
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
	log("----------------------");
	return res;
}

static int venus_create(const char *path, mode_t mode, struct fuse_file_info *fi){
	log("create file called");
	string cached_file_name;
	bool found_unique_name = false;
	while(!found_unique_name){//wow this is inefficient!!
		cached_file_name = string(*cache_dir_path).append(random_string(10));
		found_unique_name = true;
		for (std::map<string, string>::iterator it=cached_files->begin(); it!=cached_files->end(); ++it){
    			if(it->second.compare(cached_file_name) == 0){//equal
				found_unique_name = false;
				break;
			}
		}
	}
	int fd = open(cached_file_name.c_str(), fi->flags, mode);
        if (fd == -1){
                return -errno;
	}
        fi->fh = fd;
	log("new file cached at " + cached_file_name);
	cached_files->insert(std::pair<string, string>(string(path), cached_file_name));
	record_cache(string(path) + SEPARATOR + cached_file_name);
	flush_file = true; 
	return 0;
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
	log("Checking modified time ");
	long file_last_mod_local = cached_files_remote_modified->find(string(path))->second;
	long file_last_mod_remote;
	if((get_modified_timestamp(path, &file_last_mod_remote) == -1)){
		return -errno;
	}
	if(file_last_mod_remote > file_last_mod_local){
		log("File out of sync with Remote, revalidating local cache");
		if(invalidate_local_cache(path) == -1){
			return -errno;
		}
		if(read_file_into_cache(path) == -1){
			return -errno;
		}
	}
	else{
		log("File NOT out of sync reading from cache...");
	}	
	log("----------------------");
	return 0;
}

static int venus_getattr(const char *path, struct stat *stbuf)
{
	log("getattr called");
	log("path: %s", path);
	memset(stbuf, 0, sizeof(struct stat));
	if(strcmp(path, "/") == 0) {
		log("Is root dir");
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}
	std::map<string, string>::iterator cached_files_it = cached_files->find(string(path));
	log("----------------------");
	if(cached_files_it != cached_files->end()){
		stat(cached_files_it->second.c_str(), stbuf);
		log("returning success at local");
		return 0;
	}
	log("----------------------");
	if(get_remote_file_attr(path, stbuf) == -1){
		log("returning enoent");
		return -ENOENT;
	}
	else{
		log("returning success at remote");
		return 0;
	}
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

static int venus_utimens(const char *path, const struct timespec ts[2])
{
        return 0;
}


int main(int argc, char *argv[])
{
	client_id = atoi(argv[3]);
	cache_dir_path = new string(string(cache_dir).append(int_to_string(client_id)).append("/"));
	open_err_log();
	make_cache_dir();
	cached_files = new std::map<string, string>();
	cached_files_remote_modified = new std::map<string, long>();
	recover_cached_files(cached_files);
	//print_cached_files(cached_files);
	recover_mod_time(cached_files_remote_modified);
	//print_mod_time(cached_files_remote_modified);
	static struct fuse_operations venus_oper;
	venus_oper.mkdir = venus_mkdir;
	venus_oper.create = venus_create;
	venus_oper.rmdir = venus_rmdir;
	venus_oper.getattr = venus_getattr;
	venus_oper.readdir = venus_readdir;
	venus_oper.open = venus_open;
	venus_oper.unlink = venus_unlink;
	venus_oper.write = venus_write;
	venus_oper.read = venus_read;
	venus_oper.chown = venus_chown;
	venus_oper.access = venus_access;
	venus_oper.getxattr = venus_getxattr;
	venus_oper.utimens = venus_utimens;
	//venus_oper.fsync = venus_fsync;
	venus_oper.release = venus_release;
	venus_oper.flush = venus_flush;
	venus_oper.statfs = venus_statfs;
	std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("192.168.1.126:50051", grpc::InsecureCredentials());
	//std::shared_ptr<grpc::Channel> channel = grpc::CreateChannel("128.105.32.140:50051", grpc::InsecureCredentials());
	stub_ = RpcService::NewStub(channel);
	argc = 3;
	//why doesnt writes to my local cache persist between client restarts?
	//clean up any temp files
	//repopulate data str
	return fuse_main(argc, argv, &venus_oper, NULL);
}
