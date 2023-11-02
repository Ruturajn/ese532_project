#include "common.h"

void dedup(ChunkData *data) {
    if (!data->is_chunk_reference)
        lzw(data->buf, data->chunk_length);
    // Here if it is a chunk reference, then send out the chunk id.
}