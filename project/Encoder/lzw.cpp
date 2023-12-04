#include <cstdint>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

//****************************************************************************************************************
#define CAPACITY                                                               \
    8192 // hash output is 15 bits, and we have 1 entry per bucket, so capacity
         // is 2^15
// #define CAPACITY 4096
//  try  uncommenting the line above and commenting line 6 to make the hash
//  table smaller and see what happens to the number of entries in the assoc mem
//  (make sure to also comment line 27 and uncomment line 28)
#define SEED 524057
#define ASSOCIATE_MEM_STORE 64
// #define CLOSEST_PRIME 65497
// #define CLOSEST_PRIME 32749
// #define CLOSEST_PRIME 65497
#define FILE_SIZE 4096

using namespace std;

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

    // if (key % 2 == 0)
    // 	h = (h - 1) + (h << 3) ^ (0x23234523);

    h ^= murmur_32_scramble(k);
    /* Finalize. */
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h & 0x1FFF;
    // return key % CLOSEST_PRIME;
}

// unsigned int my_hash(unsigned long str)
// {
//     unsigned long hash = 5381;
//     int c = 0;
//
//     while (c < 64) {
//         hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
// 		c *= c;
// 	}
//
//     return hash;
// }

// unsigned int my_hash(unsigned long key)
// {
//     key &= 0xFFFFF; // make sure the key is only 20 bits
//
//     unsigned int hashed = 0;
//
//     for(int i = 0; i < 20; i++)
//     {
//         hashed += (key >> i)&0x01;
//         hashed += hashed << 10;
//         hashed ^= hashed >> 6;
//     }
//     hashed += hashed << 3;
//     hashed ^= hashed >> 11;
//     hashed += hashed << 15;
//     return hashed & 0x7FFF;          // hash output is 15 bits
//     //return hashed & 0xFFF;
// }

void hash_lookup(unsigned long (*hash_table)[2], unsigned int key, bool *hit,
                 unsigned int *result) {
    // std::cout << "hash_lookup():" << std::endl;
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
        // std::cout << "\thit the hash" << std::endl;
        // std::cout << "\t(k,v,h) = " << key << " " << value << " " <<
        // my_hash(key) << std::endl;
    } else {
        lookup = hash_table[hash_val][1];

        // [valid][value][key]
        stored_key = lookup & 0xFFFFF;       // stored key is 20 bits
        value = (lookup >> 20) & 0xFFF;      // value is 12 bits
        valid = (lookup >> (20 + 12)) & 0x1; // valid is 1 bit
        if (valid && (key == stored_key)) {
            *hit = 1;
            *result = value;
        } else {
            *hit = 0;
            *result = 0;
        }
        // std::cout << "\tmissed the hash" << std::endl;
    }
}

void hash_insert(unsigned long (*hash_table)[2], unsigned int key,
                 unsigned int value, bool *collision) {
    // std::cout << "hash_insert():" << std::endl;
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned int hash_val = my_hash(key);

    unsigned long lookup = hash_table[hash_val][0];
    unsigned int valid = (lookup >> (20 + 12)) & 0x1;

    if (valid) {
        lookup = hash_table[hash_val][1];
        valid = (lookup >> (20 + 12)) & 0x1;
        if (valid) {
            *collision = 1;
        } else {
            hash_table[hash_val][1] = (1UL << (20 + 12)) | (value << 20) | key;
            *collision = 0;
        }
        // std::cout << "\tKey is:" << key << std::endl;
        // std::cout << "\tcollision in the hash" << std::endl;
    } else {
        hash_table[hash_val][0] = (1UL << (20 + 12)) | (value << 20) | key;
        *collision = 0;
        // std::cout << "\tinserted into the hash table" << std::endl;
        // std::cout << "\t(k,v,h) = " << key << " " << value << " " <<
        // my_hash(key) << std::endl;
    }
}
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
    unsigned int value[ASSOCIATE_MEM_STORE]; // value store is 64 deep, because
                                             // the lookup mems are 64 bits wide
    unsigned int fill; // tells us how many entries we've currently stored
} assoc_mem;

// cast to struct and use ap types to pull out various feilds.

void assoc_insert(assoc_mem *mem, unsigned int key, unsigned int value,
                  bool *collision) {
    // std::cout << "assoc_insert():" << std::endl;
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    if (mem->fill < ASSOCIATE_MEM_STORE) {
        mem->upper_key_mem[(key >> 18) % 512] |=
            (1 << mem->fill); // set the fill'th bit to 1, while preserving
                              // everything else
        mem->middle_key_mem[(key >> 9) % 512] |=
            (1 << mem->fill); // set the fill'th bit to 1, while preserving
                              // everything else
        mem->lower_key_mem[(key >> 0) % 512] |=
            (1 << mem->fill); // set the fill'th bit to 1, while preserving
                              // everything else
        mem->value[mem->fill] = value;
        mem->fill++;
        *collision = 0;
        // std::cout << "\tinserted into the assoc mem" << std::endl;
        // std::cout << "\t(k,v) = " << key << " " << value << std::endl;
    } else {
        *collision = 1;
        // std::cout << "\tcollision in the assoc mem" << std::endl;
    }
}

void assoc_lookup(assoc_mem *mem, unsigned int key, bool *hit,
                  unsigned int *result) {
    // std::cout << "assoc_lookup():" << std::endl;
    key &= 0xFFFFF; // make sure key is only 20 bits

    unsigned int match_high = mem->upper_key_mem[(key >> 18) % 512];
    unsigned int match_middle = mem->middle_key_mem[(key >> 9) % 512];
    unsigned int match_low = mem->lower_key_mem[(key >> 0) % 512];

    unsigned int match = match_high & match_middle & match_low;

    unsigned int address = 0;
    for (; address < ASSOCIATE_MEM_STORE; address++) {
        if ((match >> address) & 0x1) {
            break;
        }
    }
    if (address != ASSOCIATE_MEM_STORE) {
        *result = mem->value[address];
        *hit = 1;
        // std::cout << "\thit the assoc" << std::endl;
        // std::cout << "\t(k,v) = " << key << " " << *result << std::endl;
    } else {
        *hit = 0;
        // std::cout << "\tmissed the assoc" << std::endl;
    }
}
//****************************************************************************************************************
void insert(unsigned long (*hash_table)[2], assoc_mem *mem, unsigned int key,
            unsigned int value, bool *collision) {
    hash_insert(hash_table, key, value, collision);
    if (*collision) {
        assoc_insert(mem, key, value, collision);
    }
}

void lookup(unsigned long (*hash_table)[2], assoc_mem *mem, unsigned int key,
            bool *hit, unsigned int *result) {
    hash_lookup(hash_table, key, hit, result);
    if (!*hit) {
        assoc_lookup(mem, key, hit, result);
    }
}

// void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx,
//          uint32_t *lzw_codes, uint32_t *code_length, uint8_t *failure,
// 		 unsigned int *associative_mem) {
//     // create hash table and assoc mem
//     unsigned long hash_table[CAPACITY][2];
//     assoc_mem my_assoc_mem;
//
//     // make sure the memories are clear
//     for(int i = 0; i < CAPACITY; i++)
//     {
//         hash_table[i][0] = 0;
//         hash_table[i][1] = 0;
//     }
//     my_assoc_mem.fill = 0;
//     for(int i = 0; i < 512; i++)
//     {
//         my_assoc_mem.upper_key_mem[i] = 0;
//         my_assoc_mem.middle_key_mem[i] = 0;
//         my_assoc_mem.lower_key_mem[i] = 0;
//     }
//
// //    for (int i = 0; i < ASSOCIATE_MEM_STORE; i++)
// //        my_assoc_mem.value[i] = 0;
//
//     // init the memories with the first 256 codes
//     for(unsigned long i = 0; i < 256; i++)
//     {
//         bool collision = 0;
//         unsigned int key = (i << 8) + 0UL; // lower 8 bits are the next char,
//         the upper bits are the prefix code insert(hash_table, &my_assoc_mem,
//         key, i, &collision);
//     }
//     int next_code = 256;
//
//     unsigned int prefix_code = (unsigned int)chunk[start_idx];
//     unsigned int code = 0;
//     unsigned char next_char = 0;
// 	uint32_t j = 0;
//
//     int i = start_idx;
//     while(i < end_idx)
//     {
//         if(i + 1 == end_idx)
//         {
// 			lzw_codes[j] = prefix_code;
// 			*code_length = j + 1;
//             break;
//         }
//         next_char = chunk[i + 1];
//
//         bool hit = 0;
//         lookup(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char,
//         &hit, &code); if(!hit)
//         {
// 			lzw_codes[j] = prefix_code;
//
//             bool collision = 0;
//             insert(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char,
//             next_code, &collision); if(collision)
//             {
// 				printf("FAILED TO INSERT!!\n");
//             	*failure = 1;
//                 return;
//             }
//             next_code += 1;
// 			++j;
//             prefix_code = next_char;
//         }
//         else
//         {
//             prefix_code = code;
//         }
//         i += 1;
//     }
// 	*failure = 0;
//     *associative_mem = my_assoc_mem.fill;
// }

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx,
         uint32_t *lzw_codes, uint32_t *code_length, uint8_t *failure,
         unsigned int *associative_mem) {
    // create hash table and assoc mem
    unsigned long hash_table[CAPACITY][2];
    assoc_mem my_assoc_mem;

    *failure = 0;

    // make sure the memories are clear
    for (int i = 0; i < CAPACITY; i++) {
        hash_table[i][0] = 0;
        hash_table[i][1] = 0;
    }
    my_assoc_mem.fill = 0;
    for (int i = 0; i < 512; i++) {
        my_assoc_mem.upper_key_mem[i] = 0;
        my_assoc_mem.middle_key_mem[i] = 0;
        my_assoc_mem.lower_key_mem[i] = 0;
    }

    // init the memories with the first 256 codes
    for (unsigned long i = 0; i < 256; i++) {
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

    for (uint32_t i = start_idx; i < end_idx - 1; i++) {
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
            insert(hash_table, &my_assoc_mem, concat_val, next_code,
                   &collision);
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

// void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx,
//          uint16_t *lzw_codes, uint32_t *code_length) {
// 	//cout << "Encoding\n";
// 	unordered_map<string, int> table;
// 	for (int i = 0; i <= 255; i++) {
// 		string ch = "";
// 		ch += char(i);
// 		table[ch] = i;
// 	}
//
// 	string p = "", c = "";
// 	p += chunk[start_idx];
// 	int code = 256;
// 	int j = 0;
// 	// cout << "String\tlzw_codes\tAddition\n";
// 	for (int i = start_idx; i < end_idx; i++) {
// 		if (i != end_idx - 1)
// 			c += chunk[i + 1];
// 		if (table.find(p + c) != table.end()) {
// 			p = p + c;
// 		}
// 		else {
// 			// cout << p << "\t" << table[p] << "\t\t"
// 			// 	<< p + c << "\t" << code << endl;
// 			// lzw_codes.push_back(table[p]);
// 			lzw_codes[j] = table[p];
// 			table[p + c] = code;
// 			code++;
// 			++j;
// 			p = c;
// 		}
// 		c = "";
// 	}
// 	// cout << p << "\t" << table[p] << endl;
// 	// lzw_codes.push_back(table[p]);
// 	lzw_codes[j] = table[p];
// 	*code_length = j + 1;
// }
