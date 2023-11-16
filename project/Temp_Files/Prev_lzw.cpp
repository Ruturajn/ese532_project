#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

//****************************************************************************************************************
#define CAPACITY                                                               \
    65536 // hash output is 15 bits, and we have 1 entry per bucket, so capacity
          // is 2^15
// #define CAPACITY 4096
//  try  uncommenting the line above and commenting line 6 to make the hash
//  table smaller and see what happens to the number of entries in the assoc mem
//  (make sure to also comment line 27 and uncomment line 28)
#define SEED 524057
#define ASSOCIATE_MEM_STORE 64
// #define CLOSEST_PRIME 65497
#define CLOSEST_PRIME 32749
#define FILE_SIZE 4096

using namespace std;

// static inline uint32_t murmur_32_scramble(uint32_t k) {
//     k *= 0xcc9e2d51;
//     k = (k << 15) | (k >> 17);
//     k *= 0x1b873593;
//     return k;
// }
//
// unsigned int my_hash(unsigned long key)
// {
// 	uint32_t h = SEED;
//     uint32_t k = key;
//     h ^= murmur_32_scramble(k);
//     h = (h << 13) | (h >> 19);
//     h = h * 5 + 0xe6546b64;
//
//     h ^= murmur_32_scramble(k);
//     /* Finalize. */
// 	h ^= h >> 16;
// 	h *= 0x85ebca6b;
// 	h ^= h >> 13;
// 	h *= 0xc2b2ae35;
// 	h ^= h >> 16;
//     return h & 0xFFFF;
// 	// return key % CLOSEST_PRIME;
// }

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
    if (key % 2 == 0)
        h = h >> 4;
    else
        h << 3;

    h ^= murmur_32_scramble(k);
    /* Finalize. */
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h & 0xFFFF;
    // return key % CLOSEST_PRIME;
}

// unsigned int my_hash(unsigned long str)
// {
//     unsigned long hash = 5381;
//     int c = 1;
//
//     while (c < 64) {
//         hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
//         c++;
// 	}
//
//     return (hash >> 3) & 0xFFFF;
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

void hash_lookup(unsigned long *hash_table, unsigned int key, bool *hit,
                 unsigned int *result) {
    // std::cout << "hash_lookup():" << std::endl;
    key &= 0xFFFFF; // make sure key is only 20 bits

    unsigned long lookup = hash_table[my_hash(key)];

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
        *hit = 0;
        *result = 0;
        // std::cout << "\tmissed the hash" << std::endl;
    }
}

void hash_insert(unsigned long *hash_table, unsigned int key,
                 unsigned int value, bool *collision) {
    // std::cout << "hash_insert():" << std::endl;
    key &= 0xFFFFF; // make sure key is only 20 bits
    value &= 0xFFF; // value is only 12 bits

    unsigned long lookup = hash_table[my_hash(key)];
    unsigned int valid = (lookup >> (20 + 12)) & 0x1;

    if (valid) {
        *collision = 1;
        // std::cout << "\tKey is:" << key << std::endl;
        // std::cout << "\tcollision in the hash" << std::endl;
    } else {
        hash_table[my_hash(key)] = (1UL << (20 + 12)) | (value << 20) | key;
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
void insert(unsigned long *hash_table, assoc_mem *mem, unsigned int key,
            unsigned int value, bool *collision) {
    hash_insert(hash_table, key, value, collision);
    if (*collision) {
        assoc_insert(mem, key, value, collision);
    }
}

void lookup(unsigned long *hash_table, assoc_mem *mem, unsigned int key,
            bool *hit, unsigned int *result) {
    hash_lookup(hash_table, key, hit, result);
    if (!*hit) {
        assoc_lookup(mem, key, hit, result);
    }
}

void lzw(unsigned char *chunk, uint32_t start_idx, uint32_t end_idx,
         uint32_t *lzw_codes, uint32_t *code_length, uint8_t *failure,
         unsigned int *associative_mem) {
    // create hash table and assoc mem
    unsigned long hash_table[CAPACITY];
    assoc_mem my_assoc_mem;

    // make sure the memories are clear
    for (int i = 0; i < CAPACITY; i++) {
        hash_table[i] = 0;
    }
    my_assoc_mem.fill = 0;
    for (int i = 0; i < 512; i++) {
        my_assoc_mem.upper_key_mem[i] = 0;
        my_assoc_mem.middle_key_mem[i] = 0;
        my_assoc_mem.lower_key_mem[i] = 0;
    }

    for (int i = 0; i < ASSOCIATE_MEM_STORE; i++)
        my_assoc_mem.value[i] = 0;

    // init the memories with the first 256 codes
    for (unsigned long i = 0; i < 256; i++) {
        bool collision = 0;
        unsigned int key = (i << 8) + 0UL; // lower 8 bits are the next char,
                                           // the upper bits are the prefix code
        insert(hash_table, &my_assoc_mem, key, i, &collision);
    }
    int next_code = 256;

    unsigned int prefix_code = (unsigned int)chunk[start_idx];
    unsigned int code = 0;
    unsigned char next_char = 0;
    uint32_t j = 0;

    int i = start_idx;
    while (i < end_idx) {
        if (i + 1 == end_idx) {
            lzw_codes[j] = prefix_code;
            *code_length = j + 1;
            break;
        }
        next_char = chunk[i + 1];

        bool hit = 0;
        lookup(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char, &hit,
               &code);
        if (!hit) {
            lzw_codes[j] = prefix_code;

            bool collision = 0;
            insert(hash_table, &my_assoc_mem, (prefix_code << 8) + next_char,
                   next_code, &collision);
            if (collision) {
                *failure = 1;
                return;
            }
            next_code += 1;
            ++j;
            prefix_code = next_char;
        } else {
            prefix_code = code;
        }
        i += 1;
    }
    *associative_mem = my_assoc_mem.fill;
}

// "Golden" functions to check correctness
std::vector<int> encoding(std::string s1) {
    // std::cout << "Encoding\n";
    std::unordered_map<std::string, int> table;
    for (int i = 0; i <= 255; i++) {
        std::string ch = "";
        ch += char(i);
        table[ch] = i;
    }

    std::string p = "", c = "";
    p += s1[0];
    int code = 256;
    std::vector<int> output_code;
    // std::cout << "String\tOutput_Code\tAddition\n";
    for (int i = 0; i < s1.length(); i++) {
        if (i != s1.length() - 1)
            c += s1[i + 1];
        if (table.find(p + c) != table.end()) {
            p = p + c;
        } else {
            // std::cout << p << "\t" << table[p] << "\t\t"
            //      << p + c << "\t" << code << std::endl;
            output_code.push_back(table[p]);
            table[p + c] = code;
            code++;
            p = c;
        }
        c = "";
    }
    // std::cout << p << "\t" << table[p] << std::endl;
    output_code.push_back(table[p]);
    return output_code;
}

int main() {
    FILE *fptr = fopen("../Text_Files/LittlePrince.txt", "r");
    if (fptr == NULL) {
        printf("Unable to open file!\n");
        exit(EXIT_FAILURE);
    }

    size_t file_sz = FILE_SIZE;

    unsigned char *file_data = NULL;

    file_data = (unsigned char *)calloc((file_sz + 1), sizeof(unsigned char));
    if (file_data == NULL) {
        printf("Unable to allocate memory for file data!\n");
        exit(EXIT_FAILURE);
    }

    fseek(fptr, file_sz, file_sz * 3);

    size_t bytes_read = fread(file_data, 1, file_sz, fptr);

    if (bytes_read != file_sz)
        printf("Unable to read file contents");

    file_data[file_sz] = '\0';
    fclose(fptr);
    std::string s;
    char *temp = (char *)file_data;

    while (*temp != '\0') {
        s += *temp;
        temp += 1;
    }

    uint32_t lzw_codes[4096];
    uint32_t packet_len = 0;
    unsigned int fill = 0;
    uint8_t failure = 0;
    lzw(file_data, 0, file_sz, &lzw_codes[0], &packet_len, &failure, &fill);

    std::vector<int> output_code = encoding(s);

    if (failure) {
        cout << "FAILED TO INSERT INTO ASSOC MEM!!\n";
        exit(EXIT_FAILURE);
    }

    cout << packet_len << " | " << output_code.size() << endl;

    if (packet_len != output_code.size()) {
        cout << "FAILURE MISMATCHED PACKET LENGTH!!" << endl;
        cout << packet_len << "|" << output_code.size() << endl;
    }

    for (int i = 0; i < output_code.size(); i++) {
        if (output_code[i] != lzw_codes[i]) {
            cout << "FAILURE!!" << endl;
            cout << output_code[i] << "|" << lzw_codes[i] << " at i = " << i
                 << endl;
        }
    }

    cout << "Associate Memory count : " << fill << endl;

    free(file_data);
    return 0;
}