#include "common.h"
#include "sha256/sha256.h"

using namespace std;

void sha_256(unsigned char *chunked_data, uint32_t chunk_start_idx,
        uint32_t chunk_end_idx, vector<unsigned char> &sha_fingerprint) {
    SHA256_CTX ctx;
    BYTE hash_val[32];

    sha256_hash(&ctx, (const BYTE*)(chunked_data + chunk_start_idx),
                chunk_start_idx, chunk_end_idx, hash_val, 1);

    for (int i = 0; i < 32; i++) {
        sha_fingerprint[i] = hash_val[i];
#ifdef SHA_DEBUG
        printf("%02x", sha_fingerprint[i]);
#endif
    }
#ifdef SHA_DEBUG
    printf("\n");
#endif
}
