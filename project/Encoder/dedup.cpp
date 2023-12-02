#include "common.h"
#include <cstdlib>
#include <iostream>
#include <stdbool.h>
#include <unordered_map>

using namespace std;

// Return -1 on failure.
int64_t dedup(string sha_fingerprint) {

    // static vector<pair<string, int64_t>> sha_chunk_id_map;
    static unordered_map<string, int64_t> sha_chunk_id_map;
    static int64_t chunk_id = 0;

    // for (auto it: sha_chunk_id_map) {
    //     if (it.first == sha_fingerprint)
    //         return it.second;
    // }

    // sha_chunk_id_map.push_back({sha_fingerprint, chunk_id});
    // ++chunk_id;
    // return -1;

    bool found =
        (sha_chunk_id_map.find(sha_fingerprint) == sha_chunk_id_map.end()
             ? false
             : true);

    // Perform lookup in map here.
    if (!found) {
        // Insert into map before calling LZW.
        sha_chunk_id_map[sha_fingerprint] = chunk_id;
        ++chunk_id;
        return -1;
    } else
        return sha_chunk_id_map[sha_fingerprint];
}
