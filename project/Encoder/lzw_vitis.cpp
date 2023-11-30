#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <hls_stream.h>

#define CAPACITY                                                                                                   	\
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
	unsigned int fill;                     	// tells us how many entries we've currently stored
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

void inline hash_lookup(unsigned long (*hash_table)[2], unsigned int key, bool *hit, unsigned int *result) {
	key &= 0xFFFFF; // make sure key is only 20 bits

	unsigned int hash_val = my_hash(key);

	unsigned long lookup = hash_table[hash_val][0];

	// [valid][value][key]
	unsigned int stored_key = lookup & 0xFFFFF;   	// stored key is 20 bits
	unsigned int value = (lookup >> 20) & 0xFFF;  	// value is 12 bits
	unsigned int valid = (lookup >> (20 + 12)) & 0x1; // valid is 1 bit

	if (valid && (key == stored_key)) {
    	*hit = 1;
    	*result = value;
    	return;
	}

	lookup = hash_table[hash_val][1];

	// [valid][value][key]
	stored_key = lookup & 0xFFFFF;   	// stored key is 20 bits
	value = (lookup >> 20) & 0xFFF;  	// value is 12 bits
	valid = (lookup >> (20 + 12)) & 0x1; // valid is 1 bit
	if (valid && (key == stored_key)) {
    	*hit = 1;
    	*result = value;
    	return;
	}
	*hit = 0;
	*result = 0;
}

void inline hash_insert(unsigned long (*hash_table)[2], unsigned int key, unsigned int value, bool *collision) {
    key &= 0xFFFFF; // make sure key is only 20 bits
	value &= 0xFFF; // value is only 12 bits

	unsigned int hash_val = my_hash(key);

	unsigned long *lookup_ptr = hash_table[hash_val];
	unsigned long lookup = lookup_ptr[0];
	unsigned int valid = (lookup >> (20 + 12)) & 0x1;

	if (!valid) {
    	lookup_ptr[0] = (1UL << (20 + 12)) | (value << 20) | key;
    	*collision = 0;
    	return;
	}

    lookup = lookup_ptr[1];
    valid = (lookup >> (20 + 12)) & 0x1;
    if (valid) {
   	 *collision = 1;
   	 return;
    }
    lookup_ptr[1] = (1UL << (20 + 12)) | (value << 20) | key;
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

	//LOOP5:
	//	for (; address < ASSOCIATIVE_MEM_STORE; address++) {
	//    	if ((match >> address) & 0x1) {
	//        	break;
	//    	}
	//	}
}

void inline insert(unsigned long (*hash_table)[2], assoc_mem *mem, unsigned int key, unsigned int value, bool *collision) {
	hash_insert(hash_table, key, value, collision);
	if (*collision) {
    	assoc_insert(mem, key, value, collision);
	}
}

void inline lookup(unsigned long (*hash_table)[2], assoc_mem *mem, unsigned int key, bool *hit, unsigned int *result) {
	hash_lookup(hash_table, key, hit, result);
	if (!*hit) {
    	assoc_lookup(mem, key, hit, result);
	}
}

static void lzw_compute(hls::stream<unsigned char> &in_stream, uint32_t start_idx, uint32_t end_idx,
                    	hls::stream<uint32_t> &out_stream, uint32_t *code_length,
                    	uint8_t *failure, unsigned int *associative_mem) {

	// create hash table and assoc mem
	unsigned long hash_table[CAPACITY][2];
#pragma HLS array_partition variable=hash_table complete dim=2
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

	int next_code = 256;

    	unsigned int prefix_code = (unsigned int)in_stream.read();
    	unsigned int concat_val = 0;
    	unsigned int code = 0;
    	unsigned char next_char = 0;
    	uint32_t j = 0;

	LOOP4:
    for (int i = start_idx; i < end_idx - 1; i++) {
        	/* next_char = chunk[i + 1]; */
        	next_char = in_stream.read();
        	bool hit = 0;
        	//concat_val = (prefix_code << 8) + next_char;
        	//***************************************************************
        	lookup(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char, &hit, &code);
        	if (!hit) {
            	/* lzw_codes[j] = prefix_code; */
            	out_stream.write(prefix_code);

            	bool collision = 0;
            	//***************************************************************
            	insert(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char, next_code, &collision);
            	if (collision) {

                	*failure = 1;
                	//continue;
            	}
            	next_code += 1;
            	++j;
            	prefix_code = next_char;
        	} else {
            	prefix_code = code;
        	}
    	}
    	/* lzw_codes[j] = prefix_code; */
    	out_stream.write(prefix_code);
    	*code_length = j + 1;
    	*associative_mem = my_assoc_mem.fill;
}

static void load_data(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx, hls::stream<unsigned char> &in_stream) {

	for (int i = start_idx; i < end_idx; i++)
    	in_stream.write(chunk[i]);
}

static void store_data(hls::stream<uint32_t> &out_stream, uint32_t temp_code_length, uint32_t *lzw_codes, uint32_t *code_length,
                   	unsigned int temp_associative_mem, unsigned int *associative_mem, uint8_t temp_failure,
                   	uint8_t *failure) {

	for (int i = 0; i < temp_code_length; i++)
    	lzw_codes[i] = out_stream.read();

	*code_length = temp_code_length;
	*failure = temp_failure;
	*associative_mem = temp_associative_mem;
}

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx, uint32_t *lzw_codes, uint32_t *code_length,
     	uint8_t *failure, unsigned int *associative_mem) {

#pragma HLS INTERFACE m_axi port=chunk depth=4096 bundle=p0
#pragma HLS INTERFACE m_axi port=lzw_codes depth=8192 bundle=p1
#pragma HLS INTERFACE m_axi port=code_length depth=1 bundle=p1
#pragma HLS INTERFACE m_axi port=failure depth=1 bundle=p1
#pragma HLS INTERFACE m_axi port=associative_mem depth=1 bundle=p1

	hls::stream<unsigned char>in_stream("load");
	hls::stream<uint32_t>out_stream("store");

#pragma HLS STREAM variable=in_stream depth=4096
#pragma HLS STREAM variable=out_stream depth=4096

	uint32_t temp_code_length = 0;
	uint8_t temp_failure = 0;
	unsigned int temp_associative_mem = 0;

#pragma HLS DATAFLOW
	load_data(chunk, start_idx, end_idx, in_stream);
	lzw_compute(in_stream, start_idx, end_idx, out_stream, &temp_code_length, &temp_failure, &temp_associative_mem);
	store_data(out_stream, temp_code_length, lzw_codes, code_length, temp_associative_mem, associative_mem, temp_failure, failure);
}
