
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
#include "aes.h"

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
// extern "C" void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key);
// extern "C" void AES_ECB_encrypt(const struct AES_ctx* ctx, uint8_t* buf);
// extern "C" void AES_ECB_decrypt(const struct AES_ctx* ctx, uint8_t* buf);


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
	int i = 0;
	while (*ptr != ',') {
		N_buf[i] = RSA_PK[i];
		ptr++;
		i++;
	}
	N_buf[i] = '\0';
	ptr++;
	strcpy(E_buf, ptr);
	double N;
	double E;
	double K1;
	sscanf(N_buf, "%lf", &N);
	sscanf(E_buf, "%lf", &E);
	sscanf(AES_k1, "%lf", &K1);
	int RA_key = mod_exp((int) N, (int) E, (int) K1);
	int dec = 1;
	while (RA_key / 10 != 0) {
		RA_key = RA_key	/ 10;
		dec += 1;
	}
	if (dec > 16) {
		fprintf(stderr, "RA_key too long!\n");
		exit;
	}
	char key[dec];
	sprintf(key, "%d", RA_key);
	char key_pad[16];
	if (strlen(key) < 16) {
		strcpy(key_pad, key);
		char* ptr = key_pad + strlen(key);
		for (int i = 0; i < 16 - strlen(key); i++) {
			*ptr = '0';
			ptr += 1;
		}
	} else {
		strcpy(key_pad, key);
	}
	unsigned char AES_k2_uc[strlen(AES_k2)];
	for (int i = 0; i < strlen(AES_k2); i++) {
		AES_k2_uc[i] = static_cast<unsigned char>(AES_k2[i]);
	}

	unsigned char key_pad_uc[16];
	for (int i = 0; i < 16; i++) {
		key_pad_uc[i] = static_cast<unsigned char>(key_pad[i]);
	}

	AES_ctx* ctx = (AES_ctx*) malloc(sizeof(AES_ctx));
	AES_init_ctx(ctx, AES_k2_uc);
	AES_ECB_encrypt(ctx, key_pad_uc);
	fprintf(stderr, "ciphered RA key = %s\n", key_pad_uc);
	AES_ECB_decrypt(ctx, key_pad_uc);
	fprintf(stderr, "original RA key = %s\n", key_pad_uc);

	// FILE* output = fopen("ciphered_file", "w+");
	// if (output == NULL) {
	// 	fprintf(stderr, "failed to create ciphered_file!\n");	
	// }
	
	
	

	// fclose(output);
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
	// fp = fopen(argv[4], "r");
	// if (fp == NULL) {
    //     fprintf(stderr, "Could not open original file.\n");
    //     exit(1);
    // }
	enc();
	// fclose(fp);
	return 0;
}
