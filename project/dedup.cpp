#include "common.h"
#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <stdbool>

using namespace std;

void dedup(unsigned char *data, uint32_t start_idx,
                uint32_t end_idx, string sha_fingerprint,
                uint32_t *out_packet) {

    static unordered_map<string, uint64_t> sha_chunk_id_map;
    static uint64_t chunk_id = 0;

    bool found = (sha_chunk_id_map.find(sha_fingerprint) == sha_chunk_id_map.end() ? false : true);

    // Perform lookup in map here.
    if (!found) {

        // Insert into map before calling LZW.
        ++chunk_id;
        sha_chunk_id_map[sha_fingerprint] = chunk_id;

        uint16_t code_length = 0;

        lzw(data, start_idx, end_idx, out_packet, &code_length);
        return;
    }

    // Here if it is a chunk reference, then send out the chunk id.
    
}