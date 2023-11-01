#include "common.h"

void dedup(DedupData *data) {
    if (!data->is_chunk_reference)
        lzw(data->buf);
    // Here if it is a chunk reference, then send out the chunk id.
}