#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPACITY                                                               \
    16384 // hash output is 15 bits, and we have 1 entry per bucket, so capacity
          // is 2^15
#define SEED 524057
#define ASSOCIATIVE_MEM_STORE 64
#define CHUNK_SIZE 4096
#define MAX_CHUNK_SIZE 8192
#define INFO_LEN 4
#define BUFFER_LEN 16384
#define MAX_ITERATIONS 20
#define MAX_OUTPUT_CODE_SIZE 40960
#define CODE_LENGTH 12
#define MAX_BIT_PACKET_DATA (MAX_OUTPUT_CODE_SIZE * 4)

typedef enum InfoParams {
    INFO_START_IDX,
    INFO_END_IDX,
    INFO_OUT_PACKET_LENGTH,
    INFO_FAILURE,
} InfoParams;

//****************************************************************************************************************
typedef struct {
    // Each key_mem has a 9 bit address (so capacity = 2^9 = 512)
    // and the key is 20 bits, so we need to use 3 key_mems to cover all the key
    // bits. The output width of each of these memories is 64 bits, so we can
    // only store 64 key value pairs in our associative memory map.

    unsigned long upper_key_mem[512]; // the output of these  will be 64 bits
                                      // wide (size of unsigned long).
    unsigned long middle_key_mem[512];
    unsigned long lower_key_mem[512];
    unsigned int
        value[ASSOCIATIVE_MEM_STORE]; // value store is 64 deep, because the
                                      // lookup mems are 64 bits wide
    unsigned int fill; // tells us how many entries we've currently stored
} assoc_mem;

// cast to struct and use ap types to pull out various feilds.
//****************************************************************************************************************

static inline uint32_t murmur_32_scramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

unsigned int inline my_hash(unsigned long key) {
    uint32_t h = SEED;
    uint32_t k = key;
    h ^= murmur_32_scramble(k);
    h = (h << 13) | (h >> 19);
    h = h * 5 + 0xe6546b64;

    h ^= murmur_32_scramble(k);
    /* Finalize. */
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h & 0x3FFF;
}

void inline hash_lookup(unsigned long (*hash_table)[2], unsigned int key,
                        bool *hit, unsigned int *result) {
    key &= 0xFFFFF; // make sure key is only 20 bits

    unsigned int hash_val = my_hash(key);

    unsigned long lookup = hash_table[hash_val][0];

    // [valid][value][key]
    unsigned int stored_key = lookup & 0xFFFFF;       // stored key is 20 bits
    unsigned int value = (lookup >> 20) & 0xFFF;      // value is 12 bits
    unsigned int valid = (lookup >> (20 + 12)) & 0x1; // valid is 1 bit

    if (valid && (key == stored_key)) {
        *hit = 1;
        *result = value;
        return;
    }

    lookup = hash_table[hash_val][1];

    // [valid][value][key]
    stored_key = lookup & 0xFFFFF;       // stored key is 20 bits
    value = (lookup >> 20) & 0xFFF;      // value is 12 bits
    valid = (lookup >> (20 + 12)) & 0x1; // valid is 1 bit
    if (valid && (key == stored_key)) {
        *hit = 1;
        *result = value;
        return;
    }
    *hit = 0;
    *result = 0;
}

void inline hash_insert(unsigned long (*hash_table)[2], unsigned int key,
                        unsigned int value, bool *collision) {
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned int hash_val = my_hash(key);

    unsigned long lookup = hash_table[hash_val][0];
    unsigned int valid = (lookup >> (20 + 12)) & 0x1;

    if (!valid) {
        hash_table[hash_val][0] = (1UL << (20 + 12)) | (value << 20) | key;
        *collision = 0;
        return;
    }

    lookup = hash_table[hash_val][1];
    valid = (lookup >> (20 + 12)) & 0x1;
    if (valid) {
        *collision = 1;
        return;
    }
    hash_table[hash_val][1] = (1UL << (20 + 12)) | (value << 20) | key;
    *collision = 0;
}

void inline assoc_insert(assoc_mem *mem, unsigned int key, unsigned int value,
                         bool *collision) {
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned int mem_fill = mem->fill;

    if (mem_fill < ASSOCIATIVE_MEM_STORE) {
        unsigned int key_high = (key >> 18) & 0x1FF;
        unsigned int key_middle = (key >> 9) & 0x1FF;
        unsigned int key_low = (key)&0x1FF;
        mem->upper_key_mem[key_high] |=
            (1 << mem_fill); // set the fill'th bit to 1, while preserving
                             // everything else
        mem->middle_key_mem[key_middle] |=
            (1 << mem_fill); // set the fill'th bit to 1, while preserving
                             // everything else
        mem->lower_key_mem[key_low] |=
            (1 << mem_fill); // set the fill'th bit to 1, while preserving
                             // everything else
        mem->value[mem_fill] = value;
        mem->fill = mem_fill + 1;
        *collision = 0;
        return;
    }
    *collision = 1;
}

void inline assoc_lookup(assoc_mem *mem, unsigned int key, bool *hit,
                         unsigned int *result) {
    key &= 0xFFFFF; // make sure key is only 20 bits
    unsigned int key_high = (key >> 18) & 0x1FF;
    unsigned int key_middle = (key >> 9) & 0x1FF;
    unsigned int key_low = (key)&0x1FF;

    unsigned int match_high = mem->upper_key_mem[key_high];
    unsigned int match_middle = mem->middle_key_mem[key_middle];
    unsigned int match_low = mem->lower_key_mem[key_low];

    unsigned long match = match_high & match_middle & match_low;

    if (match == 0) {
        *hit = 0;
        return;
    }

    unsigned int address = 0; //(unsigned int)(log2(match & -match) + 1);

    // Right shift until the rightmost set bit is found
    address += ((match & 0x00000000FFFFFFFFUL) == 0) ? 32 : 0;
    match >>= ((match & 0x00000000FFFFFFFFUL) == 0) ? 32 : 0;

    address += ((match & 0x000000000000FFFFUL) == 0) ? 16 : 0;
    match >>= ((match & 0x000000000000FFFFUL) == 0) ? 16 : 0;

    address += ((match & 0x00000000000000FFUL) == 0) ? 8 : 0;
    match >>= ((match & 0x00000000000000FFUL) == 0) ? 8 : 0;

    address += ((match & 0x000000000000000FUL) == 0) ? 4 : 0;
    match >>= ((match & 0x000000000000000FUL) == 0) ? 4 : 0;

    address += ((match & 0x0000000000000003UL) == 0) ? 2 : 0;
    match >>= ((match & 0x0000000000000003UL) == 0) ? 2 : 0;

    address += ((match & 0x0000000000000001UL) == 0) ? 1 : 0;
    match >>= ((match & 0x0000000000000001UL) == 0) ? 1 : 0;

    if (address != ASSOCIATIVE_MEM_STORE) {
        *result = mem->value[address];
        *hit = 1;
        return;
    }
    *hit = 0;
}

void inline insert(unsigned long (*hash_table)[2], assoc_mem *mem,
                   unsigned int key, unsigned int value, bool *collision) {
    hash_insert(hash_table, key, value, collision);
    if (*collision) {
        assoc_insert(mem, key, value, collision);
    }
}

void inline lookup(unsigned long (*hash_table)[2], assoc_mem *mem,
                   unsigned int key, bool *hit, unsigned int *result) {
    hash_lookup(hash_table, key, hit, result);
    if (!*hit) {
        assoc_lookup(mem, key, hit, result);
    }
}

static void compute_lzw(unsigned char *input, uint32_t *lzw_codes,
                        uint32_t generic_info[4], uint64_t offset) {

    // create hash table and assoc mem
    unsigned long hash_table[CAPACITY][2];
    assoc_mem my_assoc_mem;

#pragma HLS array_partition variable=hash_table complete dim=2

    // make sure the memories are clear
LOOP1:
    for (int i = 0; i < CAPACITY; i++) {
        hash_table[i][0] = 0;
        hash_table[i][1] = 0;
    }
    my_assoc_mem.fill = 0;

LOOP2:
    for (int i = 0; i < 512; i++) {
        my_assoc_mem.upper_key_mem[i] = 0;
        my_assoc_mem.middle_key_mem[i] = 0;
        my_assoc_mem.lower_key_mem[i] = 0;
    }

    int next_code = 256;
    uint8_t failure = 0;
    uint32_t start_idx = generic_info[INFO_START_IDX];
    uint32_t end_idx = generic_info[INFO_END_IDX];
    unsigned int prefix_code = (unsigned int)input[start_idx];
    unsigned int code = 0;
    unsigned char next_char = 0;
    uint64_t j = offset;

LOOP3:
    for (int i = start_idx; i < end_idx - 1; i++) {
        next_char = input[i + 1];
        bool hit = 0;
        lookup(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char, &hit,
               &code);
        if (!hit) {
            lzw_codes[j] = prefix_code;

            bool collision = 0;
            insert(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char,
                   next_code, &collision);
            if (collision) {
                failure = 1;
            }
            next_code += 1;
            ++j;
            prefix_code = next_char;
        } else {
            prefix_code = code;
        }
    }

    lzw_codes[j] = prefix_code;
    generic_info[INFO_OUT_PACKET_LENGTH] = (j - offset) + 1;
    if (failure)
        generic_info[INFO_FAILURE] = failure;
}

void create_packet(const int out_packet_length, uint32_t* out_packet,
                   unsigned char *data, const int packet_len) {

    uint32_t data_idx = 0;
    uint16_t current_val = 0;
    int bits_left = 0;
    int current_val_bits_left = 0;
    for (uint32_t i = 0; i < out_packet_length; i++) {
        current_val = out_packet[i];
        current_val_bits_left = CODE_LENGTH;

        if (bits_left == 0 && current_val_bits_left == CODE_LENGTH) {
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
                data[data_idx] = ((current_val)&0xFF);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }
    }

}

void lzw(unsigned char input[BUFFER_LEN],
         unsigned char bit_packed_data[MAX_OUTPUT_CODE_SIZE * 4],
         uint32_t lzw_codes[MAX_OUTPUT_CODE_SIZE],
         uint32_t chunk_indices[MAX_ITERATIONS],
         uint32_t out_packet_lengths[MAX_ITERATIONS],
         int64_t dedup_out[MAX_ITERATIONS]) {

#pragma HLS INTERFACE m_axi port=input depth=16384 bundle=p0
#pragma HLS INTERFACE m_axi port=bit_packed_data depth=163840 bundle=p1
#pragma HLS INTERFACE m_axi port=lzw_codes depth=40960 bundle=p0
#pragma HLS INTERFACE m_axi port=chunk_indices depth=20 bundle=p0
#pragma HLS INTERFACE m_axi port=out_packet_lengths depth=20 bundle=p1
#pragma HLS INTERFACE m_axi port=dedup_out depth=20 bundle=p1

    uint32_t temp_lzw_codes[MAX_OUTPUT_CODE_SIZE] = {0};
    uint32_t temp_chunk_indices[MAX_ITERATIONS] = {0};
    unsigned char temp_input[BUFFER_LEN] = {0};

#pragma HLS array_partition variable=temp_lzw_codes block factor=5 dim=1
#pragma HLS array_partition variable=temp_chunk_indices block factor=10 dim=1
#pragma HLS array_partition variable=temp_input block factor=16 dim=1

    LOOP4: for (int i = 0; i < MAX_ITERATIONS; i++) {
        temp_chunk_indices[i] = chunk_indices[i];
    }

    memcpy(temp_input, input, sizeof(unsigned char) * BUFFER_LEN);

    // The first element in the chunk_indices array contains the size
    // of the chunk_indices array.
    uint32_t chunk_indices_len = chunk_indices[0];

    // This is an array that packs generic information for the LZW
    // function. The elements in the this array are as follows:
    // 0 - start_idx
    // 1 - end_idx
    // 2 - out_packet_length
    // 3 - failure
    uint32_t generic_info[INFO_LEN];
    generic_info[INFO_FAILURE] = 0;
    generic_info[INFO_OUT_PACKET_LENGTH] = 0;

    uint64_t offset = 0;
    uint64_t bit_pack_offset = 0;

    const int bound = chunk_indices_len - 1;

    unsigned char data[MAX_OUTPUT_CODE_SIZE * 4] = {0};
    int packet_len = 0;
    uint32_t header = 0;

    LOOP5: for (int i = 1; i <= MAX_ITERATIONS; i++) {
#pragma HLS UNROLL factor=2
        if (i <= bound) {
            generic_info[INFO_START_IDX] = temp_chunk_indices[i];
            generic_info[INFO_END_IDX] = temp_chunk_indices[i + 1];
            compute_lzw(temp_input, temp_lzw_codes, generic_info, offset);
            out_packet_lengths[i] = generic_info[INFO_OUT_PACKET_LENGTH];

            if (dedup_out[i - 1] == -1) {
                packet_len = ((out_packet_lengths[i] * 12) / 8);
                packet_len =
                    ((out_packet_lengths[i] & 1) != 0) ? packet_len + 1 : packet_len;
                header = packet_len << 1;

                // 0x00 00 0F 2A
                data[bit_pack_offset] = (header & 0xFF);
                data[bit_pack_offset + 1] = ((header >> 8) & 0xFF);
                data[bit_pack_offset + 2] = ((header >> 16) & 0xFF);
                data[bit_pack_offset + 3] = ((header >> 24) & 0xFF);

                bit_pack_offset += 4;

                create_packet(out_packet_lengths[i], temp_lzw_codes + offset,
                              data + bit_pack_offset, packet_len);
                bit_pack_offset += packet_len;

            } else {
                header = (dedup_out[i - 1] << 1) | 1;

                data[bit_pack_offset] = (header & 0xFF);
                data[bit_pack_offset + 1] = ((header >> 8) & 0xFF);
                data[bit_pack_offset + 2] = ((header >> 16) & 0xFF);
                data[bit_pack_offset + 3] = ((header >> 24) & 0xFF);

                bit_pack_offset += 4;
            }
            offset += generic_info[INFO_OUT_PACKET_LENGTH];
        }
    }

    memcpy(bit_packed_data, data, MAX_OUTPUT_CODE_SIZE * 4 * sizeof(unsigned char));
    memcpy(lzw_codes, temp_lzw_codes, MAX_OUTPUT_CODE_SIZE * sizeof(uint32_t));

    // The first element of the out_packet_lengths is going to signify
    // failure to insert into the associative memory.
    out_packet_lengths[0] = (bit_pack_offset << 1) | generic_info[INFO_FAILURE];
}
