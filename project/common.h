#ifndef __COMMON_H__
#define __COMMON_H__

#define MAX_BUF_SIZE 8192
#define WIN_SIZE 16
#define MODULUS 256
#define TARGET 0
#define PRIME 3

#include <vector>
#include <iostream>
#include <cstdlib>

using namespace std;

typedef struct ChunkData {
    // If set call LZW with the buffer below.
    char is_chunk_reference;

    // Length of the chunk.
    uint64_t chunk_length;

    // The buffer that will hold the chunk, if the
    // flag above is set. If it isn't then it will
    // hold the chunk ID.
    unsigned char buf[MAX_BUF_SIZE];
} ChunkData;

void cdc(unsigned char *buff, unsigned int buff_size,
         unsigned int start_idx, unsigned int end_idx,
         std::vector<uint64_t>& vect);

// Send in the whole chunk as input to the sha function, since the SHA is computed for
// the whole chunk.
uint64_t sha_256(unsigned char *chunk);

// Use a data structure that can help dedup understand if the passed parameter
// is a reference to a chunk or an actual chunk.
void dedup(ChunkData *data);

// LZW can accept chunks, so probably just pass a buffer that contains the chunk
// to be compressed.
vector<int> lzw(unsigned char *chunk, int chunk_length);

#endif