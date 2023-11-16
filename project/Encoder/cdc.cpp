#include "common.h"
#include <math.h>

using namespace std;

static uint64_t hash_func(unsigned char *input, unsigned int pos) {
    uint64_t hash = 0;
    for (int i = 0; i < WIN_SIZE; i++)
        hash += (uint64_t)input[pos + WIN_SIZE - 1 - i] * pow(PRIME, i + 1);
    return hash;
}

void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect) {

    uint64_t hash = hash_func(buff, 0);

    for (unsigned int i = 0; i < buff_size; i++) {
        if (((hash & MODULUS_MASK) == TARGET) || ((i & MODULUS_MASK) == TARGET)) {
#ifdef CDC_DEBUG
            cout << i << endl;
#endif
            vect.push_back(i);
        }
        hash = (hash * PRIME) - ((uint64_t)buff[i] * pow(PRIME, WIN_SIZE + 1)) + ((uint64_t)buff[i + WIN_SIZE] * PRIME);
    }

    vect.push_back(buff_size);
#ifdef CDC_DEBUG
    cout << buff_size - 1 << endl;
#endif
}