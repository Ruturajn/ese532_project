#include "common.h"

using namespace std;

string sha_256(unsigned char *chunk, uint32_t chunk_start_idx, uint32_t chunk_end_idx) {
    uint64_t hash = 0;

    for (int i = chunk_start_idx; i < chunk_end_idx; i++) {
        hash ^= chunk[i];
        hash >>= 13;
        hash <<= 7;
        hash += hash;
    }

    return to_string(hash);
}
