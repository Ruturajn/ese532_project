#include "common.h"
#include "sha256/sha256.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace std;

string bytearray_hex_string(unsigned char *bytes, int size) {
    stringstream ss;
    ss << hex << setfill('0');
    for (int i = 0; i < size; i++) {
        ss << setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

string sha_256(unsigned char *chunked_data, uint32_t chunk_start_idx,
               uint32_t chunk_end_idx) {
    SHA256_CTX ctx;
    BYTE hash_val[32];

    sha256_hash(&ctx, (const BYTE *)(chunked_data + chunk_start_idx),
                chunk_start_idx, chunk_end_idx, hash_val, 1);

    string sha_fingerprint = bytearray_hex_string(hash_val, 32);

#ifdef SHA_DEBUG
    printf("SHA:\n");
    // for (int i = 0; i < 32; i++) {
    //     printf("%02x", sha_fingerprint[i]);
    // }
    // printf("\n");
    cout << sha_fingerprint << endl;
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash_val[i]);
    }
    printf("\n");
#endif
    return sha_fingerprint;
}
