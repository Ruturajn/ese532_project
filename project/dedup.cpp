#include "common.h"
#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <stdbool.h>

using namespace std;

// Return -1 on failure.
uint32_t dedup(string sha_fingerprint) {

    static unordered_map<string, uint64_t> sha_chunk_id_map;
    static uint32_t chunk_id = 0;

    bool found = (sha_chunk_id_map.find(sha_fingerprint) ==
                  sha_chunk_id_map.end() ? false : true);

    // Perform lookup in map here.
    if (!found) {
        // Insert into map before calling LZW.
        ++chunk_id;
        sha_chunk_id_map[sha_fingerprint] = chunk_id;
        return -1;
    } else
        return sha_chunk_id_map[sha_fingerprint];
}
