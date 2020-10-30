
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <cmath>
#include <cerrno> 
#include <cstring>
#include <vector>
#include "aes.hpp"

using namespace std;

char* AES_k1;
char* AES_k2;
char* RSA_PK;
FILE* fp = NULL;

/*double mod_exp(double g, double e, double N){
	if (g == 0){
  		return 0;
	}else if (g == 1){
   		return 1;
	}else if (N <= 0) {
  		print('invalid modulas');
   		return 0;
	}else if (e < 0){
   		print('invalid exponent');
   		return 0;
	}
	double rest = e;
	//bits = np.array([]);
	std::vector<double> bits;
 	while (rest > 0){
   		double i = 0.0
   		while (pow(2, i) < rest){
     		i += 1;
		}
   		i -= 1;
   		rest -= pow(2,i);
		bits.push_back(i);
   		if (rest == 1) {
     		bits.push_back(0.0);
     		break;
   		} else if (rest == 0){
	     	break;
		}
	}
 	double base = g % N;
 	double lexp = bits[0]; //the largest exponent
 	std::vector<double> material;
 	for i in range(0, int(lexp) + 1)
   		exp = (int(base**(2**i))) % N
   		material = np.append(material, exp)
	for (int i = 0; i < (int) lexp + 1) {
		double exp = ((int)(pow(base, pow(2, i)))) % N;	
		material.push_back(exp);
	}
	double result = 1;
 	for exps in bits:
   		result = int((result * material[int(exps)]) % N)
	for (int i = 0; i < bits.size(); i++) {
		result = (result * material[(int)bits[i]]) %N;
	}
	
 	return result;
}*/



int mod_exp(int g, int e, int N) {
	int result = g % N;
	for (int i = 0; i < e; i++) {
		result = (result*result) % N;
	}	
	return result;
}



void enc(){
	/*
	double AES_k1_num;
	sscanf(AES_k1, "%lf", &AES_k1_num);
	double p = AES_k1_num;
	double q = RSA_PK;
	double to = (p-1.0)*(q-1.0);
	*/
	char N_buf[strlen(RSA_PK)];
	char E_buf[strlen(RSA_PK)];
	char* ptr = RSA_PK;
	while (*ptr != ',') {
		N_buf[ptr] = RSA_PK[ptr];
		ptr++;
	}
	N_buf[ptr] = '\0';
	ptr++;
	strcpy(E_buf, ptr);
	double N;
	double E;
	double K1;
	sscanf(N_buf, "%lf", &N);
	sscanf(E_buf, "%lf", &E);
	sscanf(AES_k1, "%lf", &K1);
	int RA_key = mod_exp((int) N, (int) E, (int) K1);


	FILE* output = fopen("ciphered_file", "w+");
	if (output == NULL) {
		fprintf(stderr, "failed to create ciphered_file!\n");	
	}
	
	
	

	fclose(output);
	return;
}




int main(int argc, char** argv) {
	
	if (argc != 5) {
        fprintf(stderr, "usage: %s AES_key1 AES_key2 RSA_PK filename", argv[0]);
        exit(1);
    }

	if (strlen((char*)argv[1]) != 16 || strlen((char*)argv[2]) != 16) {
		fprintf(stderr, "Illegal AES key length!");
		exit(1);
	}
	AES_k1 = argv[1];
	AES_k2 = argv[2];
	fp = fopen(argv[4], "r");
	if (fp == NULL) {
        fprintf(stderr, "Could not open original file.\n");
        exit(1);
    }
	enc();
	fclose(fp);
	return;
}
