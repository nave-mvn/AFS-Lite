#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

using namespace std;

int main(){
	FILE * pFile;
	long lSize;
	char buffer[1024];
	size_t result;

   	std::ofstream myfile;
   	myfile.open("output.txt");
	pFile = fopen ("small_file.txt" , "rb");
	if (pFile==NULL) {fputs ("File error",stderr); exit (1);}
	for (;;) {
    		size_t n = fread(buffer, 1, 1024, pFile);
    		printf("%s", buffer);
   		myfile.write(buffer, n);
    		if (n < 1024) { break; }
	}
	myfile.close();
	
}
