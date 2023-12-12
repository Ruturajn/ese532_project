#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define LZW_CODES_LEN 100
#define CODE_LENGTH 13

int main() {

    // 0000 0000 1111 1111 // 0000 1011 1101 1111 // 0000 0111 1101 0000 // 0000 0011 1110 1000 // 0000 0000 0000 0001
    // 0001 0110 1001 0110 // 0001 1010 0000 1010 // 0000 1011 0101 0001 // 0001 0110 0000 1110

    //
    // 0 0000 1111 1111 // 0 1011 1101 1111 // 0 0111 1101 0000 // 0 0011 1110 1000 // 0 0000 0000 0001 /**/ 1 0110 1001 0110 // 1 1010 0000 1010
    // 0000 0111 1111 1010 1111 0111 1100 1111 1010 0000 0011 1110 1000 0000 0000 0000 1101 1010 0101 1011 0100 0001 0100 1011 0101 0001 // 1011 0000 0111 0
    // 0    7    f     a    f    7    C   f    a     0    3    E    8    0    0    0   D     A    5    B    4    1    4    b    5    1       B    0    7
    // 0 1
    // uint32_t codes_arr[LZW_CODES_LEN] = { 255, 3039, 2000, 1000, 1, 5782, 6666, 2897, 5646};

    // 0000 0000 0101 0100 // 0000 0000 0110 1000 // 0000 0000 0110 0101 // 0000 0000 0010 0000 // 0000 0000 0100 1100 // 0000 0000 0110 1001 // 0000 0000 0111 0100 // 0000 0000 0111 0100 // 0000 0000 0110 1100
    // 0 0000 0101 0100 // 0 0000 0110 1000 // 0 0000 0110 0101 // 0 0000 0010 0000 // 0 0000 0100 1100 // 0 0000 0110 1001 // 0 0000 0111 0100 // 0 0000 0111 0100 // 0 0000 0110 1100
    // 0000 0010 1010 0000 0001 1010 0000 0000 1100 1010 0000 0010 0000 0000 0010 0110 0000 0001 1010 0100 0000 1110 1000 0000 0111 0100 0000 0011 0110 0000
    // 0       2  A    0    1   A    0     0    C    A    0    2    0    0    2    6    0    1    A    4     0   E    8    0   7     4    0     3   6    0

    // 0 0000 0101 0100 // 0 0000 0110 1000 // 0 0000 0110 0101 // 0 0000 0010 0000 // 0 0000 0100 1100 // 0 0000 0110 1001 // 0 0000 0111 0100 // 0 0000 0110 1100
    // 0000 0010 1010 0000 0001 1010 0000 0000 1100 1010 0000 0010 0000 0000 0010 0110 0000 0001 1010 0100 0000 1110 1000 0000 0110 1100
    // 0       2  A    0    1   A    0     0    C    A    0    2    0    0    2    6    0    1    A    4     0   E    8    0    6    C
    // uint32_t codes_arr[LZW_CODES_LEN] = { 84, 104, 101, 32, 76, 105, 116, 116, 108};
    // uint32_t codes_arr[LZW_CODES_LEN] = { 255, 3039, 2000, 1000, 1, 1024, 4444, 56, 56, 678};
    uint32_t codes_arr[LZW_CODES_LEN];
    for (int i = 0; i < LZW_CODES_LEN; i++) {
        codes_arr[i] = rand() % 8192;
    }
    int data_len = ((LZW_CODES_LEN * 13) / 8) + 1;

    unsigned char *data = (unsigned char *)calloc(data_len, sizeof(unsigned char));
    if (data == NULL) {
        printf("Unable to allocate memory for data packet!");
        exit(EXIT_FAILURE);
    }

    int data_idx = 0;
    uint16_t current_val = 0;
    int bits_left = 0;
    int current_val_bits_left = 0;

    for (int i = 0; i < LZW_CODES_LEN; i++) {
        current_val = codes_arr[i];
        current_val_bits_left = CODE_LENGTH;

        if (bits_left == 0 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] = (current_val >> 5) & 0xFF;
            bits_left = 0;
            current_val_bits_left = 5;
            data_idx += 1;
        }

        if (bits_left == 0 && current_val_bits_left == 5) {
            if (data_idx < data_len) {
                data[data_idx] = (current_val & 0x1F) << 3;
                bits_left = 3;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }

        if (bits_left == 3 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] |= ((current_val >> 10) & 0x07);
            bits_left = 0;
            data_idx += 1;
            current_val_bits_left = 10;
        }

        if (bits_left == 0 && current_val_bits_left == 10) {
            if (data_idx < data_len) {
                data[data_idx] = ((current_val >> 2) & 0xFF);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 2;
            } else
                break;
        }

        if (bits_left == 0 && current_val_bits_left == 2) {
            if (data_idx < data_len) {
                data[data_idx] = (current_val & 0x03) << 6;
                bits_left = 6;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }

        if (bits_left == 6 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] |= ((current_val >> 7) & 0x3F);
            bits_left = 0;
            data_idx += 1;
            current_val_bits_left = 7;
        }

        if (bits_left == 0 && current_val_bits_left == 7) {
            if (data_idx < data_len) {
                data[data_idx] = (current_val & 0x7F) << 1;
                bits_left = 1;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }

        if (bits_left == 1 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] |= ((current_val >> 12) & 0x1);
            bits_left = 0;
            data_idx += 1;
            current_val_bits_left = 12;
        }

        if (bits_left == 0 && current_val_bits_left == 12) {
            if (data_idx < data_len) {
                data[data_idx] = ((current_val >> 4) & 0xFF);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 4;
            } else
                break;
        }

        if (bits_left == 0 && current_val_bits_left == 4) {
            if (data_idx < data_len) {
                data[data_idx] = (current_val & 0x0F) << 4;
                bits_left = 4;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }

        if (bits_left == 4 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] |= ((current_val >> 9) & 0x0F);
            data_idx += 1;
            bits_left = 0;
            current_val_bits_left = 9;
        }

        if (bits_left == 0 && current_val_bits_left == 9) {
            if (data_idx < data_len) {
                data[data_idx] = ((current_val >> 1) & 0xFF);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 1;
            } else
                break;
        }

        if (bits_left == 0 && current_val_bits_left == 1) {
            data[data_idx] = (current_val & 0x01) << 7;
            bits_left = 7;
            current_val_bits_left = 0;
            continue;
        }

        if (bits_left == 7 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] |= ((current_val >> 6) & 0x7F);
            bits_left = 0;
            current_val_bits_left = 6;
            data_idx += 1;
        }

        if (bits_left == 0 && current_val_bits_left == 6) {
            if (data_idx < data_len) {
                data[data_idx] = ((current_val) & 0x3F) << 2;
                bits_left = 2;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }

        if (bits_left == 2 && current_val_bits_left == CODE_LENGTH) {
            if (data_idx < data_len) {
                data[data_idx] |= ((current_val >> 11) & 0x03);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 11;
            } else
                break;
        }

        if (bits_left == 0 && current_val_bits_left == 11) {
            if (data_idx < data_len) {
                data[data_idx] = ((current_val >> 3) & 0xFF);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 3;
            } else
                break;
        }

        if (bits_left == 0 && current_val_bits_left == 3) {
            if (data_idx < data_len) {
                data[data_idx] = ((current_val) & 0x07) << 5;
                bits_left = 5;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }

        if (bits_left == 5 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] |= ((current_val >> 8) & 0x1F);
            bits_left = 0;
            current_val_bits_left = 8;
            data_idx += 1;
        }

        if (bits_left == 0 && current_val_bits_left == 8) {
            if (data_idx < data_len) {
                data[data_idx] = (current_val & 0xFF);
                bits_left = 0;
                current_val_bits_left = 0;
                data_idx += 1;
                continue;
            } else
                break;
        }

        if (current_val_bits_left > 0)
            printf("YEP YEP YEPE YPEYPYE!!!\n");
    }


    printf("Data Length : %d\n", data_idx);

    /* for (int i = 0; i < data_len; i++) */
    /*     printf("%x\n", data[i]); */

    putchar('\n');

    free(data);

    return 0;
}
