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
#define MODULUS 256
#define MODULUS_MASK 0xFFF // This is used when performing modulus with 4096.
#define TARGET 0
#define PRIME 3

#define CHECK_MALLOC(ptr, msg) \
    if (ptr == NULL) {         \
        printf("%s\n", msg);   \
        exit(EXIT_FAILURE);    \
    }                          \

using namespace std;

void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect);

void sha_256(unsigned char *chunked_data, uint32_t chunk_start_idx,
             uint32_t chunk_end_idx, vector<unsigned char>&sha_fingerprint);

int64_t dedup(vector<unsigned char> sha_fingerprint);

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx, uint16_t *lzw_codes,
         uint32_t *code_length);

#endif
