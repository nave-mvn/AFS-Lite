#include <string>
#include <sstream>
#include <iostream>
#include <fstream>
#include <time.h>
#include <algorithm>
#include <stdarg.h>  // For va_start, etc.
#include <memory>    // For std::unique_ptr
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <vector>

#define BUF_SIZE 4 * 1024

int client_id;

using namespace std;

void print_cached_files(std::map<string, string>* cached_files){
	for (std::map<string, string>::iterator it=cached_files->begin(); it!=cached_files->end(); ++it){
    		std::cout << it->first << " => " << it->second << '\n';
	}
}

void print_mod_time(std::map<string, long>* cached_files){
	for (std::map<string, long>::iterator it=cached_files->begin(); it!=cached_files->end(); ++it){
    		std::cout << it->first << " => " << it->second << '\n';
	}
}

string int_to_string(int a){
	stringstream ss;
	ss << a;
	return ss.str();
}

template <class T>
std::string any_to_string (const T& t)
{
    std::stringstream ss;
    ss << t;
    return ss.str();
}

void open_cache_log(){
	std::ofstream ofs;
	ofs.open("/tmp/cache"+int_to_string(client_id)+".txt", std::ofstream::out | std::ofstream::trunc);
	ofs.close();
}

void open_err_log(){
	std::ofstream ofs;
	ofs.open("/tmp/err"+int_to_string(client_id)+".txt", std::ofstream::out | std::ofstream::trunc);
	ofs.close();
}

void recover_cached_files(std::map<string, string>* cached_files){
	ifstream input("/tmp/cache"+int_to_string(client_id)+".txt");
	if (!input.good()) {
		return;
	}
	for(std::string line; getline(input, line);){
		int del_index = line.find("!");
		if(del_index == 0){
			string file_name = line.substr(1, line.size());
			std::map<string, string>::iterator cached_files_it = cached_files->find(file_name);
			if(cached_files_it != cached_files->end()){
				cached_files->erase(cached_files_it);
			}
			continue;
		}
		int sep_index = line.find("::");
		string file_name = line.substr(0, sep_index);
		string cached_name = line.substr(sep_index+2, line.size());
		std::map<string, string>::iterator cached_files_it = cached_files->find(file_name);
		if(cached_files_it == cached_files->end()){
			cached_files->insert(std::make_pair(file_name, cached_name));
		}
		else{
			cached_files_it->second = cached_name;
		}
	}	
}

void recover_mod_time(std::map<string, long>* cached_mod_time){
	ifstream input("/tmp/cache_mod"+int_to_string(client_id)+".txt");
	if (!input.good()) {
		return;
	}
	for(std::string line; getline(input, line);){
		int del_index = line.find("!");
		if(del_index == 0){
			string file_name = line.substr(1, line.size());
			std::map<string, long>::iterator cached_mod_it = cached_mod_time->find(file_name);
			if(cached_mod_it != cached_mod_time->end()){
				cached_mod_time->erase(cached_mod_it);
			}
			continue;
		}
		int sep_index = line.find("::");
		string file_name = line.substr(0, sep_index);
		long mod_time = stol(line.substr(sep_index+2, line.size()), NULL);
		std::map<string, long>::iterator mod_time_it = cached_mod_time->find(file_name);
		if(mod_time_it == cached_mod_time->end()){
			cached_mod_time->insert(std::make_pair(file_name, mod_time));
		}
		else{
			mod_time_it->second = mod_time;
		}
	}	
}

void record_mod_time(const std::string fmt_str, ...) {
	int final_n, n = ((int)fmt_str.size()) * 2; /* Reserve two times as much as the length of the fmt_str */
	std::string str;
	std::unique_ptr<char[]> formatted;
	va_list ap;
	while(1) {
		formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
		strcpy(&formatted[0], fmt_str.c_str());
		va_start(ap, fmt_str);
		final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
		va_end(ap);
		if (final_n < 0 || final_n >= n)
			n += abs(final_n - n + 1);
		else
			break;
	}
	string toPrint = std::string(formatted.get());
	ofstream err_file;
	err_file.open ("/tmp/cache_mod"+int_to_string(client_id)+".txt", ios::in|ios::app);
	err_file << toPrint << std::endl;
	err_file.close();
}

void record_cache(const std::string fmt_str, ...) {
	int final_n, n = ((int)fmt_str.size()) * 2; /* Reserve two times as much as the length of the fmt_str */
	std::string str;
	std::unique_ptr<char[]> formatted;
	va_list ap;
	while(1) {
		formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
		strcpy(&formatted[0], fmt_str.c_str());
		va_start(ap, fmt_str);
		final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
		va_end(ap);
		if (final_n < 0 || final_n >= n)
			n += abs(final_n - n + 1);
		else
			break;
	}
	string toPrint = std::string(formatted.get());
	ofstream err_file;
	err_file.open ("/tmp/cache"+int_to_string(client_id)+".txt", ios::in|ios::app);
	err_file << toPrint << std::endl;
	err_file.close();
}

// Its is difficult to debug the FS calls when the FS runs in the background
// So open a file and log debugging info it
void log(const std::string fmt_str, ...) {
	int final_n, n = ((int)fmt_str.size()) * 2; /* Reserve two times as much as the length of the fmt_str */
	std::string str;
	std::unique_ptr<char[]> formatted;
	va_list ap;
	while(1) {
		formatted.reset(new char[n]); /* Wrap the plain char array into the unique_ptr */
		strcpy(&formatted[0], fmt_str.c_str());
		va_start(ap, fmt_str);
		final_n = vsnprintf(&formatted[0], n, fmt_str.c_str(), ap);
		va_end(ap);
		if (final_n < 0 || final_n >= n)
			n += abs(final_n - n + 1);
		else
			break;
	}
	string toPrint = std::string(formatted.get());
	ofstream err_file;
	err_file.open ("/tmp/err"+int_to_string(client_id)+".txt", ios::in|ios::app);
	err_file << toPrint << std::endl;
	err_file.close();
}

int copy_file(const char* original, const char* dest){
	log("Copying file %s to %s", original, dest);
	FILE *pFile = fopen(original, "rb");
	if (pFile == NULL) {
		log("Error in opening original file"); 
		return -1;
	}
	char buffer[BUF_SIZE];
	std::ofstream outfile;
	outfile.open(dest);
	for(;;){
		size_t n = fread(buffer, 1, BUF_SIZE, pFile);
		outfile.write(buffer, n);
		if (n < BUF_SIZE) { 
			break; 
		}
	}
	outfile.close();
	fclose(pFile);
	return 0;
}

timespec diff(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}

std::string random_string( size_t length )
{
	auto randchar = []() -> char
	{
		const char charset[] =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t max_index = (sizeof(charset) - 1);
		return charset[ rand() % max_index ];
	};
	std::string str(length,0);
	std::generate_n( str.begin(), length, randchar );
	return str;
}
