#ifndef __COMMON_H__
#define __COMMON_H__

#include <vector>
#include <iostream>
#include <cstdlib>
#include <stdio.h>
#include <stdbool.h>

#define MAX_CHUNK_SIZE 8192
#define WIN_SIZE 16
#define CODE_LENGTH 12
#define MODULUS 2048
#define MODULUS_MASK 0x7FF // This is used when performing modulus with MODULUS.
#define TARGET 0
#define PRIME 3

#define CHECK_MALLOC(ptr, msg) \
    if (ptr == NULL) {         \
        printf("%s\n", msg);   \
        exit(EXIT_FAILURE);    \
    }                          \

using namespace std;

void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect);

string sha_256(unsigned char *chunked_data, uint32_t chunk_start_idx,
             uint32_t chunk_end_idx);

int64_t dedup(string sha_fingerprint);

// void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx, uint16_t *lzw_codes,
//          uint32_t *code_length);

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx,
         uint32_t *lzw_codes, uint32_t *code_length, uint8_t *failure,
		 unsigned int *associative_mem);

#endif
