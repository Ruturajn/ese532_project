#ifndef __COMMON_H__
#define __COMMON_H__

#define MAX_BUF_SIZE 8192

typedef struct DedupData {
    // If set call LZW with the buffer below.
    char is_chunk_reference;

    // The buffer that will hold the chunk, if the
    // flag above is set. If it isn't then it will
    // hold the chunk ID.
    unsigned char buf[MAX_BUF_SIZE];
} DedupData;

void cdc(unsigned char *buff, unsigned int buff_size,
         unsigned int start_idx, unsigned int end_idx,
         std::vector<uint64_t>& vect);

// Send in the whole chunk as input to the sha function, since the SHA is computed for
// the whole chunk.
uint64_t sha(unsigned char *chunk);

// Use a data structure that can help dedup understand if the passed parameter
// is a reference to a chunk or an actual chunk.
void dedup(DedupData *data);

// LZW can accept chunks, so probably just pass a buffer that contains the chunk
// to be compressed.
vector<int> lzw(unsigned char *chunk);

#endif