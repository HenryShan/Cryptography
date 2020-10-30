#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <cerrno> 
#include <cstring>
#include "aes.hpp"

using namespace std;

char* AES_k2;
double RSA_p;
double RSA_q;
FILE* fp = NULL;

void dec(){
}

int main(int argc, char** argv) {
	
	// if (argc != 5) {
    //     fprintf(stderr, "usage: %s AES_key2 RSAp RSAq filename", argv[0]);
    //     exit(1);
    // }

	// if (strlen((char*)argv[1]) != 16) {
	// 	fprintf(stderr, "Illegal AES key length!");
	// 	exit(1);
	// }
	// sscanf(argv[2], "%lf", &RSA_p);
	// sscanf(argv[3], "%lf", &RSA_q);
	// if (fp == NULL) {
    //     fprintf(stderr, "Could not open original file.\n");
    //     exit(1);
    // }
	// dec();
	// fclose(fp);
	return;
}
