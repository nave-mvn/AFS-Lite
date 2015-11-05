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

#define BUF_SIZE 4 * 1024

int client_id;

using namespace std;

string int_to_string(int a){
	stringstream ss;
	ss << a;
	return ss.str();
}

void open_log(){
	std::ofstream ofs;
	ofs.open("/tmp/afs"+int_to_string(client_id)+".txt", std::ofstream::out | std::ofstream::trunc);
	ofs.close();
}

void open_err_log(){
	std::ofstream ofs;
	ofs.open("/tmp/err"+int_to_string(client_id)+".txt", std::ofstream::out | std::ofstream::trunc);
	ofs.close();
}

void record(const std::string fmt_str, ...) {
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
	err_file.open ("/tmp/afs"+int_to_string(client_id)+".txt", ios::in|ios::app);
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
