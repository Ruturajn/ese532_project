#include "common.h"
#include "sha256/sha256.h"

using namespace std;

string sha_256(unsigned char *chunked_data, uint32_t chunk_start_idx,
               uint32_t chunk_end_idx) {
    SHA256_CTX ctx;
    BYTE hash_val[32];

    sha256_hash(&ctx, (const BYTE*)(chunked_data + chunk_start_idx),
                chunk_start_idx, chunk_end_idx, hash_val, 1);

    // for (int i = 0; i < 32; i++)
    //     printf("%02x", hash_val[i]);

    // printf("\n");
    // char *temp = (char *)hash_val;
    // for (int i = 0; i < 32; i++)
    //     printf("%02x", hash_val[i]);

    // printf("\n");

    string hash((char *)hash_val);

    // for (int i = 0; i < 32; i++)
    //     printf("%02x", hash[i]);

    // printf("\n");

    return hash;
}
