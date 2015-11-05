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

using namespace std;

int main(int argc, char *argv[]){
	
	if(argc < 2){
		cout<<"Too few Arguments!!"<<endl;
		return 0;
	}

	// open file for read
	ifstream input(argv[1]);
	if(!input.good()){
		cout<<"File does not exist!!"<<endl;
		return 0;
	}
	
	cout<<"--------Printing file at open---------"<<endl;
	// print contents of file
	for(std::string line; getline(input, line);){
		cout<<line<<endl;
	}
	cout<<"--------------------------------------"<<endl;
	input.close();	

	// open file for append
	FILE* writefile = fopen(argv[1], "a");
	if(0 != fseek(writefile, 0, SEEK_END)){
		cout<<"fseek failed"<<endl;
		return 0;
	}

	for(int i=0; i<3; ++i){
		char c;
		cout << "Enter an alphabet: ";
  		cin >> c;
		char buf[10000];
		for(int j=0; j<10000; ++j){
			buf[j] = c;
		}
		fwrite(buf, 1, 10000, writefile);	
	}

	fclose(writefile);
	// if receive a number, then crash
}
