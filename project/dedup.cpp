#include "common.h"
#include <iostream>
#include <cstdlib>
#include <unordered_map>
#include <stdbool.h>

using namespace std;

void dedup(unsigned char *data, uint32_t start_idx,
                uint32_t end_idx, string sha_fingerprint,
                uint16_t *out_packet) {

    static unordered_map<string, uint64_t> sha_chunk_id_map;
    static uint32_t chunk_id = 0;
    uint32_t temp_chunk_id = 0;
    uint32_t out_packet_length = 0;

    bool found = (sha_chunk_id_map.find(sha_fingerprint) ==
                  sha_chunk_id_map.end() ? false : true);

    // Perform lookup in map here.
    if (!found) {

        // Insert into map before calling LZW.
        ++chunk_id;
        sha_chunk_id_map[sha_fingerprint] = chunk_id;

        lzw(data, start_idx, end_idx, out_packet, &out_packet_length);
    } else
        temp_chunk_id = sha_chunk_id_map[sha_fingerprint];

    // Send out the data packet.
    // | 0 | 1:31 [compressed chunk length in bytes or chunk index] | 9 byte data |
    unsigned char *data_packet = (unsigned char *)calloc(CODE_LENGTH, sizeof(unsigned char));
    CHECK_MALLOC(data_packet, "Unable to allocate memory for new data packet");

    if (found) {
        // Configre the Header.
        // Set the 0th bit of byte 0 to signify duplicate chunk.
        // 0x00 00 00 00
        data_packet[0] = ((temp_chunk_id >> 24) & 0xFF) | 1;
        data_packet[1] = (temp_chunk_id >> 16) & 0xFF;
        data_packet[2] = (temp_chunk_id >> 8) & 0xFF;
        data_packet[3] = (temp_chunk_id & 0xFF);
    } else {
        data_packet[0] = ((out_packet_length >> 24) & 0xFF);
        data_packet[1] = (out_packet_length >> 16) & 0xFF;
        data_packet[2] = (out_packet_length >> 8) & 0xFF;
        data_packet[3] = (out_packet_length & 0xFF);
    }

    for (int i = 4; i < CODE_LENGTH; i += 2) {
        if ((i - 4) < out_packet_length) {
            data_packet[i] = (out_packet[i] >> 8) & 0xFF;
            data_packet[i+1] = (out_packet[i] & 0xFF);
        }
    }

    for (int i = 0; i < CODE_LENGTH; i++)
        printf("%hhX ", data_packet[i]);
}
