#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define CODE_LENGTH 12

#define CHECK_MALLOC(ptr, msg) \
    if (ptr == NULL) {         \
        printf("%s\n", msg);   \
        exit(EXIT_FAILURE);    \
    }                          \

static unsigned char *create_packet(int32_t chunk_idx, uint32_t out_packet_length, uint16_t *out_packet) {
    // Send out the data packet.
    // | 31:1  [compressed chunk length in bytes or chunk index] | 0 | 9 byte data |
    uint32_t packet_len = (4 + ((out_packet_length * 12) / 8));
    packet_len = (chunk_idx == -1 && (out_packet_length % 2 != 0)) ? packet_len + 1 : packet_len;
    printf("%d\n", packet_len);
    unsigned char *data = (unsigned char *)calloc(packet_len, sizeof(unsigned char));
    CHECK_MALLOC(data, "Unable to allocate memory for new data packet");

    if (chunk_idx != -1) {
        // Configure the Header.
        // Set the 0th bit of byte 4 to signify duplicate chunk.
        // 0x00 00 00 00
        data[0] = ((chunk_idx >> 24) & 0xFF);
        data[1] = (chunk_idx >> 16) & 0xFF;
        data[2] = (chunk_idx >> 8) & 0xFF;
        data[3] = (chunk_idx & 0xFF) | 1;
    } else {
        data[0] = ((out_packet_length >> 24) & 0xFF);
        data[1] = (out_packet_length >> 16) & 0xFF;
        data[2] = (out_packet_length >> 8) & 0xFF;
        data[3] = (out_packet_length & 0xFF);
    }

    if (chunk_idx == -1) {
        int data_idx = 4;
        uint16_t current_val = 0;
        int bits_left = 0;
        int current_val_bits_left = 0;

        for (int i = 0; i < out_packet_length; i++) {
            current_val = out_packet[i];
            current_val_bits_left = CODE_LENGTH;

            if (bits_left == 0 && current_val_bits_left == CODE_LENGTH) {
                // 0000| 0101 0101 0101
                // 0000| 0000 0000 1111
                // 0000| 0000 0000 0101
                // 0000| 0000 0101 0000
                data[data_idx] = (current_val >> 4) & 0xFF;
                bits_left = 0;
                current_val_bits_left = 4;
                data_idx += 1;
            }

            if (bits_left == 0 && current_val_bits_left == 4) {
                if (data_idx < packet_len) {
                    data[data_idx] = (current_val & 0x0F) << 4;
                    bits_left = 4;
                    current_val_bits_left = 0;
                    continue;
                } else
                    break;
            }

            if (bits_left == 4 && current_val_bits_left == CODE_LENGTH) {
                data[data_idx] |= ((current_val >> 8) & 0x0F);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 8;
            }

            if (bits_left == 0 && current_val_bits_left == 8) {
                if (data_idx < packet_len) {
                    data[data_idx] = ((current_val) & 0xFF);
                    bits_left = 0;
                    data_idx += 1;
                    current_val_bits_left = 0;
                    continue;
                } else
                    break;
            }
        }
    } 
    return data;
}

int main() {

    int32_t chunk_idx = -1;
    uint32_t out_packet_length = 7;
    uint16_t out_packet[7] = {4023, 34, 777, 90, 54, 1000, 2399};
    // uint16_t out_packet[2] = {4023, 34};
    unsigned char *data_packet = create_packet(chunk_idx, out_packet_length, out_packet);

    for (int i = 0; i < 15; i++) {
        printf("%x - %d\n", data_packet[i], i);
    }

    putchar('\n');

    return 0;
}