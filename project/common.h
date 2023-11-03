#ifndef __COMMON_H__
#define __COMMON_H__

#include <vector>
#include <iostream>
#include <cstdlib>
#include <stdio.h>

#define MAX_CHUNK_SIZE 8192
#define WIN_SIZE 16
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

// Send in the whole chunk as input to the sha function, since the SHA is computed for
// the whole chunk.
string sha_256(unsigned char *chunk, uint32_t chunk_start_idx, uint32_t chunk_end_idx);

// Use a data structure that can help dedup understand if the passed parameter
// is a reference to a chunk or an actual chunk.
void dedup(unsigned char *data, uint32_t start_idx, uint32_t end_idx,
           string sha_fingerprint, uint32_t *out_packet);

// LZW can accept chunks, so probably just pass a buffer that contains the chunk
// to be compressed.
void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx, uint32_t *lzw_codes,
         uint16_t *code_length);

#endif