#include "common.h"
#include <math.h>

using namespace std;

static uint64_t hash_func(unsigned char *input, unsigned int pos) {
        uint64_t hash = 0;
        for (int i = 0; i < WIN_SIZE; i++)
                hash += input[pos+WIN_SIZE-1-i]*pow(PRIME, i+1);
        return hash;
}

void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect) {

    uint64_t hash = hash_func(buff, 0);

    for (unsigned int i = 0; i < buff_size; i++) {
        if ((hash & MODULUS_MASK) == TARGET) {
                cout << i << endl;
                vect.push_back(i);
        }
        hash = (hash * PRIME) -
               (buff[i] * pow(PRIME, WIN_SIZE + 1)) +
               (buff[i + WIN_SIZE] * PRIME);
    }
}
