#include <iostream>
#include <fstream>
#include <time.h>
#include <algorithm>

using namespace std;

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

void open_err_log(){
	std::ofstream ofs;
	ofs.open("/tmp/err.txt", std::ofstream::out | std::ofstream::trunc);
	ofs.close();
}

// Its is difficult to debug the FS calls when the FS runs in the background
// So open a file and log debugging info it
void log_err(string error){
	ofstream err_file;
  	err_file.open ("/tmp/err.txt", ios::in|ios::app);
  	err_file << "Writing this to a file ioidofidoif.\n";
  	err_file.close();
}
