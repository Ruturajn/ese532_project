#include "common.h"
#include <iostream>
#include <cstdlib>
#include <map>
#include <stdbool.h>

using namespace std;

// Return -1 on failure.
int64_t dedup(vector<unsigned char> sha_fingerprint) {

    static map<vector<unsigned char>, int64_t> sha_chunk_id_map;
    static int64_t chunk_id = 0;

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
