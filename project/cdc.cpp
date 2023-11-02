#include "common.h"

using namespace std;

static uint64_t hash_func(unsigned char *input, unsigned int pos) {
        uint64_t hash = 0;
        for (int i = 0; i < WIN_SIZE; i++)
                hash += input[pos+WIN_SIZE-1-i]*pow(PRIME, i+1);
        return hash;
}

void cdc(unsigned char *buff, unsigned int buff_size,
         unsigned int start_idx, unsigned int end_idx,
         std::vector<uint64_t>& vect) {

    uint64_t hash = hash_func(buff, start_idx);

    for (unsigned int i = start_idx; i < end_idx; i++){
        if (hash % MODULUS == TARGET)
                vect.push_back(i);
        hash = (hash * PRIME) -
                (buff[i] * pow(PRIME, WIN_SIZE + 1)) +
                (buff[i + WIN_SIZE] * PRIME);
    }
}