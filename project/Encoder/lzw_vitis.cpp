#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hls_stream.h>

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

#define SEED_MURMUR 524057
#define SEED_CRAP 75768593

#define FNV_PRIME 16777619
#define FNV_OFFSET_BASIS 2166136261U

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
    unsigned long
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

unsigned int inline murmur_hash(unsigned long key) {
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
    return h;
}

uint32_t inline Crap8(unsigned int key) {
    #define c8fold( a, b, y, z ) { p = (uint32_t)(a) * (uint64_t)(b); y ^= (uint32_t)p; z ^= (uint32_t)(p >> 32); }
    #define c8mix( in ) { h *= m; c8fold( in, m, k, h ); }

    const uint32_t m = 0x83d2e73b, n = 0x97e1cc59;
    const uint32_t key4[4] = { ((key) & 0xFF), ((key >> 8) & 0xFF), ((key >> 16) & 0xFF), ((key >> 24) & 0xFF)};
    uint32_t h = SEED_CRAP, k = n;
    uint64_t p;

    c8mix(key4[0])
    c8mix(key4[1])
    c8mix(key4[2])
    c8mix( key4[3] & ( ( 1 << ( 2 * 8 ) ) - 1 ) )
    c8fold( h ^ k, n, k, k )
    return k;
}

unsigned int fnv1a_hash(unsigned int key) {
    uint32_t hash = FNV_OFFSET_BASIS;

    hash ^= ((key >> 24) & 0xFF);
    hash *= FNV_PRIME;

    hash ^= ((key >> 16) & 0xFF);
    hash *= FNV_PRIME;

    hash ^= ((key >> 8) & 0xFF);
    hash *= FNV_PRIME;

    hash ^= ((key) & 0xFF);
    hash *= FNV_PRIME;

    return hash;
}

unsigned int djb2_hash(unsigned int key) {
    unsigned int hash = 5381;
    int c;

    hash = ((hash << 5) + hash) + ((key >> 24) & 0xFF); // hash * 33 + c
    hash = ((hash << 5) + hash) + ((key) & 0xFF); // hash * 33 + c
    hash = ((hash << 5) + hash) + ((key >> 8) & 0xFF); // hash * 33 + c
    hash = ((hash << 5) + hash) + ((key >> 24) & 0xFF); // hash * 33 + c

    return hash;
}

unsigned int my_hash(unsigned long key) {
    unsigned int hash_1 = murmur_hash(~key);
    // unsigned int hash_2 = Crap8(hash_1);
    // unsigned int hash_1 = djb2_hash(~(key));
    // unsigned int hash_2 = fnv1a_hash(key);
    // return ((((hash_2 >> 3) & 0x3F) << 7) | (((hash_3 >> 5) & 0x7) << 4) | ((hash_1 >> 7) & 0xF));
    // return ((((hash_2 >> 3) & 0x7F) << 7) |  ((hash_1 >> 7) & 0x7F));
    // return ((hash_1 + (SEED_CRAP ^ hash_2)) >> 7) & (CAPACITY_MOD);
    return hash_1 & (CAPACITY - 1);
}

void inline hash_lookup(unsigned long (*hash_table)[2], unsigned int key,
                        bool *hit, unsigned int *result) {

    key &= 0x1FFFFF; // make sure key is only 21 bits

    unsigned int hash_val = my_hash(key);

    unsigned long lookup = hash_table[hash_val][0];

    // [valid][value][key]
    unsigned long stored_key = lookup & 0x1FFFFF;       // stored key is 21 bits
    unsigned long value = (lookup >> 21) & 0x1FFF;      // value is 13 bits
    unsigned long valid = (lookup >> (21 + 13)) & 0x1; // valid is 1 bit

    if (valid && (key == stored_key)) {
        *hit = 1;
        *result = value;
        //*is_exists = 1;
        return;
    }

    lookup = hash_table[hash_val][1];

    // [valid][value][key]
    stored_key = lookup & 0x1FFFFF;       // stored key is 21 bits
    value = (lookup >> 21) & 0x1FFF;      // value is 13 bits
    valid = (lookup >> (21 + 13)) & 0x1; // valid is 1 bit
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

    key &= 0x1FFFFF; // make sure key is only 21 bits
    value &= 0x1FFF; // value is only 13 bits

    unsigned int hash_val = my_hash(key);

    unsigned long lookup = hash_table[hash_val][0];
    unsigned long valid = (lookup >> (21 + 13)) & 0x1;

    if (!valid) {
        hash_table[hash_val][0] = (1UL << (21 + 13)) | ((unsigned long)(value) << 21) | key;
        *collision = 0;
        return;
    }

    lookup = hash_table[hash_val][1];
    valid = (lookup >> (21 + 13)) & 0x1;
    if (valid) {
        *collision = 1;
        return;
    }
    hash_table[hash_val][1] = (1UL << (21 + 13)) | ((unsigned long)(value) << 21) | key;
    *collision = 0;
}

void inline assoc_insert(assoc_mem *mem, unsigned int key, unsigned int value,
                         bool *collision) {
    key &= 0x1FFFFF; // make sure key is only 21 bits
    value &= 0x1FFF; // value is only 13 bits

    unsigned int mem_fill = mem->fill;

    if (mem_fill < ASSOCIATIVE_MEM_STORE) {
        unsigned int key_high = (key >> 18) & 0x1FF;
        unsigned int key_middle = (key >> 9) & 0x1FF;
        unsigned int key_low = (key)&0x1FF;
        mem->upper_key_mem[key_high] |=
            (1UL << mem_fill); // set the fill'th bit to 1, while preserving
                             // everything else
        mem->middle_key_mem[key_middle] |=
            (1UL << mem_fill); // set the fill'th bit to 1, while preserving
                             // everything else
        mem->lower_key_mem[key_low] |=
            (1UL << mem_fill); // set the fill'th bit to 1, while preserving
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
    key &= 0x1FFFFF; // make sure key is only 21 bits
    unsigned int key_high = (key >> 18) & 0x1FF;
    unsigned int key_middle = (key >> 9) & 0x1FF;
    unsigned int key_low = (key)&0x1FF;

    unsigned long match_high = mem->upper_key_mem[key_high];
    unsigned long match_middle = mem->middle_key_mem[key_middle];
    unsigned long match_low = mem->lower_key_mem[key_low];

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

static void compute_lzw(hls::stream<unsigned char>&input_stream, hls::stream<uint32_t>&out_stream,
                        uint32_t generic_info[4]) {

    // create hash table and assoc mem
    unsigned long hash_table[CAPACITY][2];
    assoc_mem my_assoc_mem;

#pragma HLS array_partition variable=hash_table complete dim=2

    // make sure the memories are clear
    LOOP1: for (int i = 0; i < CAPACITY; i++) {
#pragma HLS UNROLL factor=512
        hash_table[i][0] = 0;
        hash_table[i][1] = 0;
    }
    my_assoc_mem.fill = 0;

    LOOP2: for (int i = 0; i < 512; i++) {
#pragma HLS UNROLL
        my_assoc_mem.upper_key_mem[i] = 0;
        my_assoc_mem.middle_key_mem[i] = 0;
        my_assoc_mem.lower_key_mem[i] = 0;
    }

    unsigned int next_code = 256;
    uint8_t failure = 0;
    uint32_t start_idx = generic_info[INFO_START_IDX];
    uint32_t end_idx = generic_info[INFO_END_IDX];
    // unsigned int prefix_code = (unsigned int)input[start_idx];
    unsigned int prefix_code = input_stream.read();
    unsigned int code = 0;
    unsigned char next_char = 0;
    uint64_t j = 0;

    LOOP3: for (int i = start_idx; i < end_idx - 1; i++) {
        // next_char = input[i + 1];
    	next_char = input_stream.read();
        bool hit = 0;
        lookup(hash_table, &my_assoc_mem, ((prefix_code << 8) + next_char), &hit,
               &code);
        if (!hit) {
            // lzw_codes[j] = prefix_code;
        	out_stream.write(prefix_code);
            bool collision = 0;
            insert(hash_table, &my_assoc_mem, ((prefix_code << 8) + next_char),
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

    // lzw_codes[j] = prefix_code;
    out_stream.write(prefix_code);
    generic_info[INFO_OUT_PACKET_LENGTH] = j + 1;
    if (failure)
        generic_info[INFO_FAILURE] = failure;
}

static void store_data(hls::stream<uint32_t> &out_stream,
					   uint32_t *lzw_codes) {
	unsigned int i = 0;

	while (!out_stream.empty()) {
		if (i < MAX_OUTPUT_CODE_SIZE) {
			lzw_codes[i] = out_stream.read();
			i++;
		}
	}
}

static void load_data(unsigned char input[BUFFER_LEN],
					  hls::stream<unsigned char> &input_stream) {
	// input_stream.reset();
	for (int i = 0; i < BUFFER_LEN; i++) {
		input_stream.write(input[i]);
	}
}

static void compute_data(hls::stream<unsigned char> &input_stream,
						 hls::stream<uint32_t> &out_stream,
						 uint32_t out_packet_lengths[MAX_ITERATIONS],
						 uint32_t temp_chunk_indices[MAX_ITERATIONS]) {

    // This is an array that packs generic information for the LZW
    // function. The elements in the this array are as follows:
    // 0 - start_idx
    // 1 - end_idx
    // 2 - out_packet_length
    // 3 - failure
    uint32_t generic_info[INFO_LEN];
    generic_info[INFO_FAILURE] = 0;
    generic_info[INFO_OUT_PACKET_LENGTH] = 0;


    // The first element in the chunk_indices array contains the size
    // of the chunk_indices array.
    uint32_t chunk_indices_len = temp_chunk_indices[0];

    // uint64_t offset = 0;

    const int bound = chunk_indices_len - 1;

    for (int i = 1; i <= MAX_ITERATIONS; i++) {
#pragma HLS UNROLL factor=2
        if (i <= bound) {
            generic_info[INFO_START_IDX] = temp_chunk_indices[i];
            generic_info[INFO_END_IDX] = temp_chunk_indices[i + 1];
            compute_lzw(input_stream, out_stream, generic_info);
            out_packet_lengths[i] = generic_info[INFO_OUT_PACKET_LENGTH];
            //offset += generic_info[INFO_OUT_PACKET_LENGTH];
        }
    }

    while(!input_stream.empty()) {
    	input_stream.read();
    }

    // The first element of the out_packet_lengths is going to signify
    // failure to insert into the associative memory.
    out_packet_lengths[0] = generic_info[INFO_FAILURE];
}

static void perform_lzw(unsigned char input[BUFFER_LEN],
						uint32_t temp_chunk_indices[MAX_ITERATIONS],
						uint32_t *lzw_codes,
						uint32_t out_packet_lengths[MAX_ITERATIONS]) {


	hls::stream<unsigned char> in_stream("chunk_in");
	hls::stream<uint32_t> out_stream("lzw_out");

#pragma HLS STREAM variable=in_stream depth=16384
#pragma HLS STREAM variable=out_stream depth=40960

//#pragma HLS DATAFLOW
	load_data(input, in_stream);
	compute_data(in_stream, out_stream, out_packet_lengths, temp_chunk_indices);
	store_data(out_stream, lzw_codes);
}

void lzw(unsigned char input[BUFFER_LEN],
         uint32_t *lzw_codes,
         uint32_t chunk_indices[MAX_ITERATIONS],
         uint32_t out_packet_lengths[MAX_ITERATIONS]) {

#pragma HLS INTERFACE m_axi port=input depth=16384 bundle=p0
#pragma HLS INTERFACE m_axi port=lzw_codes depth=40960 bundle=p1
#pragma HLS INTERFACE m_axi port=chunk_indices depth=20 bundle=p0
#pragma HLS INTERFACE m_axi port=out_packet_lengths depth=20 bundle=p0

    uint32_t temp_chunk_indices[MAX_ITERATIONS] = {0};

#pragma HLS array_partition variable=temp_chunk_indices block factor=10 dim=1

    LOOP4: for (int i = 0; i < MAX_ITERATIONS; i++) {
#pragma HLS UNROLL
        temp_chunk_indices[i] = chunk_indices[i];
    }

    perform_lzw(input, temp_chunk_indices, lzw_codes, out_packet_lengths);
};
