#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CAPACITY                                                                                                       \
    8192 // hash output is 15 bits, and we have 1 entry per bucket, so capacity
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

unsigned int my_hash(unsigned long key) {
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
    return h & 0x1FFF;
}

void hash_lookup(unsigned long (*hash_table)[2], unsigned int key, bool *hit, unsigned int *result) {
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

void hash_insert(unsigned long (*hash_table)[2], unsigned int key, unsigned int value, bool *collision) {
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned int hash_val = my_hash(key);

    unsigned long *lookup_ptr = hash_table[hash_val];
    unsigned long lookup = lookup_ptr[0];
    unsigned int valid = (lookup >> (20 + 12)) & 0x1;

    if (valid) {
        lookup = lookup_ptr[1];
        valid = (lookup >> (20 + 12)) & 0x1;
        if (valid) {
            *collision = 1;
            return;
        }
        lookup_ptr[1] = (1UL << (20 + 12)) | (value << 20) | key;
        *collision = 0;
        return;
    }

    lookup_ptr[0] = (1UL << (20 + 12)) | (value << 20) | key;
    *collision = 0;
}

void assoc_insert(assoc_mem *mem, unsigned int key, unsigned int value, bool *collision) {
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned int mem_fill = mem->fill;

    if (mem_fill < ASSOCIATIVE_MEM_STORE) {
        mem->upper_key_mem[(key >> 18) % 512] |= (1 << mem_fill); // set the fill'th bit to 1, while preserving
                                                                  // everything else
        mem->middle_key_mem[(key >> 9) % 512] |= (1 << mem_fill); // set the fill'th bit to 1, while preserving
                                                                  // everything else
        mem->lower_key_mem[(key >> 0) % 512] |= (1 << mem_fill);  // set the fill'th bit to 1, while preserving
                                                                  // everything else
        mem->value[mem_fill] = value;
        mem->fill = mem_fill + 1;
        *collision = 0;
        return;
    }
    *collision = 1;
}

void assoc_lookup(assoc_mem *mem, unsigned int key, bool *hit, unsigned int *result) {
    key &= 0xFFFFF; // make sure key is only 20 bits

    unsigned int match_high = mem->upper_key_mem[(key >> 18) % 512];
    unsigned int match_middle = mem->middle_key_mem[(key >> 9) % 512];
    unsigned int match_low = mem->lower_key_mem[(key >> 0) % 512];

    unsigned int match = match_high & match_middle & match_low;

    unsigned int address = 0;
LOOP5:
    for (; address < ASSOCIATIVE_MEM_STORE; address++) {
        if ((match >> address) & 0x1) {
            break;
        }
    }
    if (address != ASSOCIATIVE_MEM_STORE) {
        *result = mem->value[address];
        *hit = 1;
        return;
    }
    *hit = 0;
}

void insert(unsigned long (*hash_table)[2], assoc_mem *mem, unsigned int key, unsigned int value, bool *collision) {
    hash_insert(hash_table, key, value, collision);
    if (*collision) {
        assoc_insert(mem, key, value, collision);
    }
}

void lookup(unsigned long (*hash_table)[2], assoc_mem *mem, unsigned int key, bool *hit, unsigned int *result) {
    hash_lookup(hash_table, key, hit, result);
    if (!*hit) {
        assoc_lookup(mem, key, hit, result);
    }
}

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx, uint32_t *lzw_codes, uint32_t *code_length,
         uint8_t *failure, unsigned int *associative_mem) {

#pragma HLS INTERFACE m_axi port = chunk depth = 4096 bundle = p0
#pragma HLS INTERFACE m_axi port = lzw_codes depth = 8192 bundle = p1
#pragma HLS INTERFACE m_axi port = code_length depth = 1 bundle = p1
#pragma HLS INTERFACE m_axi port = failure depth = 1 bundle = p0
#pragma HLS INTERFACE m_axi port = associative_mem depth = 1 bundle = p1

    // create hash table and assoc mem
    unsigned long hash_table[CAPACITY][2];
#pragma HLS array_partition block factor = 2 dim = 1
    assoc_mem my_assoc_mem;

    *failure = 0;

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

// init the memories with the first 256 codes
LOOP3:
    for (unsigned long i = 0; i < 256; i++) {
#pragma HLS PIPELINE II = 1
        bool collision = 0;
        unsigned int key = (i << 8) + 0UL; // lower 8 bits are the next char,
                                           // the upper bits are the prefix code
        insert(hash_table, &my_assoc_mem, key, i, &collision);
    }
    int next_code = 256;

    unsigned int prefix_code = (unsigned int)chunk[start_idx];
    unsigned int concat_val = 0;
    unsigned int code = 0;
    unsigned char next_char = 0;
    uint32_t j = 0;

LOOP4:
    for (int i = start_idx; i < end_idx - 1; i++) {
        //        if(i != end_idx - 1)
        //        {
        //        	next_char = chunk[i + 1];
        //        }
        next_char = chunk[i + 1];
        bool hit = 0;
        concat_val = (prefix_code << 8) + next_char;
        lookup(hash_table, &my_assoc_mem, concat_val, &hit, &code);
        if (!hit) {
            lzw_codes[j] = prefix_code;

            bool collision = 0;
            insert(hash_table, &my_assoc_mem, concat_val, next_code, &collision);
            if (collision) {
                printf("FAILED TO INSERT!!\n");
                *failure = 1;
                continue;
            }
            next_code += 1;
            ++j;
            prefix_code = next_char;
        } else {
            prefix_code = code;
        }
    }
    lzw_codes[j] = prefix_code;
    *code_length = j + 1;
    *associative_mem = my_assoc_mem.fill;
}
