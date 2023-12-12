#ifndef __COMMON_H__
#define __COMMON_H__

#include <cstdlib>
#include <iostream>
#include <stdbool.h>
#include <stdio.h>
#include <vector>

#define MAX_CHUNK_SIZE 8192
#define MAX_OUTPUT_BUF_SIZE 40960
#define MAX_LZW_CHUNKS 20
#define CHUNK_SIZE 4096
#define WIN_SIZE 12
#define CODE_LENGTH 12
#define MODULUS CHUNK_SIZE
#define MODULUS_MASK                                                           \
    (MODULUS - 1) // This is used when performing modulus with MODULUS.
#define TARGET 0
#define PRIME 3
#define PIPELINE_BUFFER_LEN 16384

#define CHECK_MALLOC(ptr, msg)                                                 \
    if (ptr == NULL) {                                                         \
        printf("%s\n", msg);                                                   \
        exit(EXIT_FAILURE);                                                    \
    }

using namespace std;

typedef struct __attribute__((packed)) LZWData {
    uint8_t failure;          /// This variable stores the status of LZW.
    uint32_t assoc_mem_count; /// The amount of entries in the associate memory.
    uint32_t out_packet_length; /// Length of the codes array produced by LZW.
    uint32_t start_idx;
    uint32_t end_idx;
} LZWData;

void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect);

void fast_cdc(unsigned char *buff, unsigned int buff_size,
              unsigned int chunk_size, vector<uint32_t> &vect);

string sha_256(unsigned char *chunked_data, uint32_t chunk_start_idx,
               uint32_t chunk_end_idx);

int64_t dedup(string sha_fingerprint);


void lzw(unsigned char input[16384],
         uint32_t lzw_codes[40960],
         uint32_t chunk_indices[20],
         uint32_t out_packet_lengths[20]);
#endif
