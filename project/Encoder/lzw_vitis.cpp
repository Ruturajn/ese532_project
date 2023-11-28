#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <hls_stream.h>
#include "common.h"

#define CAPACITY                                                                                                       \
    16384  // hash output is 15 bits, and we have 1 entry per bucket, so capacity
         // is 2^15
#define SEED 524057
#define ASSOCIATIVE_MEM_STORE 64

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
    unsigned int value[ASSOCIATIVE_MEM_STORE]; // value store is 64 deep, because the
                                               // lookup mems are 64 bits wide
    unsigned int fill;                         // tells us how many entries we've currently stored
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

void inline hash_lookup(unsigned long *hash_table_1, unsigned long *hash_table_2, unsigned int key, bool *hit, unsigned int *result) {
    key &= 0xFFFFF; // make sure key is only 20 bits

    unsigned int hash_val = my_hash(key);

    unsigned long lookup = hash_table_1[hash_val];

    // [valid][value][key]
    unsigned int stored_key = lookup & 0xFFFFF;       // stored key is 20 bits
    unsigned int value = (lookup >> 20) & 0xFFF;      // value is 12 bits
    unsigned int valid = (lookup >> (20 + 12)) & 0x1; // valid is 1 bit

    if (valid && (key == stored_key)) {
        *hit = 1;
        *result = value;
        return;
    }

    lookup = hash_table_2[hash_val];

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

void inline hash_insert(unsigned long *hash_table_1, unsigned long *hash_table_2, unsigned int key, unsigned int value, bool *collision) {
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned int hash_val = my_hash(key);

    unsigned long lookup = hash_table_1[hash_val];
    unsigned int valid = (lookup >> (20 + 12)) & 0x1;

    if (!valid) {
        hash_table_1[hash_val] = (1UL << (20 + 12)) | (value << 20) | key;
        *collision = 0;
        return;
    }

	lookup = hash_table_2[hash_val];
	valid = (lookup >> (20 + 12)) & 0x1;
	if (valid) {
		*collision = 1;
		return;
	}
	hash_table_2[hash_val] = (1UL << (20 + 12)) | (value << 20) | key;
	*collision = 0;
}

void inline assoc_insert(assoc_mem *mem, unsigned int key, unsigned int value, bool *collision) {
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned int mem_fill = mem->fill;

    if (mem_fill < ASSOCIATIVE_MEM_STORE) {
    	unsigned int key_high = (key >> 18) & 0x1FF;
    	unsigned int key_middle = (key >> 9) & 0x1FF;
    	unsigned int key_low = (key) & 0x1FF;
        mem->upper_key_mem[key_high] |= (1 << mem_fill); // set the fill'th bit to 1, while preserving
                                                                  // everything else
        mem->middle_key_mem[key_middle] |= (1 << mem_fill); // set the fill'th bit to 1, while preserving
                                                                  // everything else
        mem->lower_key_mem[key_low] |= (1 << mem_fill);  // set the fill'th bit to 1, while preserving
                                                                  // everything else
        mem->value[mem_fill] = value;
        mem->fill = mem_fill + 1;
        *collision = 0;
        return;
    }
    *collision = 1;
}

void inline assoc_lookup(assoc_mem *mem, unsigned int key, bool *hit, unsigned int *result) {
    key &= 0xFFFFF; // make sure key is only 20 bits
	unsigned int key_high = (key >> 18) & 0x1FF;
	unsigned int key_middle = (key >> 9) & 0x1FF;
	unsigned int key_low = (key) & 0x1FF;

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

    address += ((match & 0x00000000000000FFUL) == 0) ? 8  : 0;
    match >>= ((match & 0x00000000000000FFUL) == 0) ? 8  : 0;

    address += ((match & 0x000000000000000FUL) == 0) ? 4  : 0;
    match >>= ((match & 0x000000000000000FUL) == 0) ? 4  : 0;

    address += ((match & 0x0000000000000003UL) == 0) ? 2  : 0;
    match >>= ((match & 0x0000000000000003UL) == 0) ? 2  : 0;

    address += ((match & 0x0000000000000001UL) == 0) ? 1  : 0;
    match >>= ((match & 0x0000000000000001UL) == 0) ? 1  : 0;

    if (address != ASSOCIATIVE_MEM_STORE) {
        *result = mem->value[address];
        *hit = 1;
        return;
    }
    *hit = 0;
}

void inline insert(unsigned long *hash_table_1, unsigned long *hash_table_2, assoc_mem *mem, unsigned int key, unsigned int value, bool *collision) {
    hash_insert(hash_table_1, hash_table_2, key, value, collision);
    if (*collision) {
        assoc_insert(mem, key, value, collision);
    }
}

void inline lookup(unsigned long *hash_table_1, unsigned long *hash_table_2, assoc_mem *mem, unsigned int key, bool *hit, unsigned int *result) {
    hash_lookup(hash_table_1, hash_table_2, key, hit, result);
    if (!*hit) {
        assoc_lookup(mem, key, hit, result);
    }
}

void lzw(unsigned char *input, uint32_t *lzw_codes, LZWData *data) {

#pragma HLS INTERFACE m_axi port=input depth=4096 bundle=p0
#pragma HLS INTERFACE m_axi port=lzw_codes depth=8192 bundle=p1

    // create hash table and assoc mem
    unsigned long hash_table_1[CAPACITY];
    unsigned long hash_table_2[CAPACITY];
    // unsigned long hash_table_2[CAPACITY];
    assoc_mem my_assoc_mem;

    data->failure = 0;

// make sure the memories are clear
LOOP1:
    for (int i = 0; i < CAPACITY; i++) {
        hash_table_1[i] = 0;
        hash_table_2[i] = 0;
    }
    my_assoc_mem.fill = 0;
LOOP2:
    for (int i = 0; i < 512; i++) {
        my_assoc_mem.upper_key_mem[i] = 0;
        my_assoc_mem.middle_key_mem[i] = 0;
        my_assoc_mem.lower_key_mem[i] = 0;
    }

    int next_code = 256;

    unsigned int prefix_code = (unsigned int)input[data->start_idx];
    unsigned int concat_val = 0;
    unsigned int code = 0;
    unsigned char next_char = 0;
    uint32_t j = 0;

LOOP4:
    for (int i = 0; i < (data->end_idx - data->start_idx) - 1; i++) {
        next_char = input[data->start_idx + i + 1];
        bool hit = 0;
        // concat_val = (prefix_code << 8) + next_char;
        //***************************************************************
        lookup(hash_table_1, hash_table_2, &my_assoc_mem, (prefix_code << 8) + next_char, &hit, &code);
        //***************************************************************
        if (!hit) {
            lzw_codes[j] = prefix_code;

            bool collision = 0;
            //***************************************************************
            insert(hash_table_1, hash_table_2, &my_assoc_mem, (prefix_code << 8) + next_char, next_code, &collision);
            //***************************************************************
            if (collision) {
                data->failure = 1;
                //continue;
            }
            next_code += 1;
            ++j;
            prefix_code = next_char;
        } else {
            prefix_code = code;
        }
    }
    lzw_codes[j] = prefix_code;
    data->out_packet_length = j + 1;
    data->assoc_mem_count = my_assoc_mem.fill;
}
