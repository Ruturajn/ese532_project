#include "common.h"
#include <cstdint>
#include <math.h>

using namespace std;

uint64_t pow_primes[18] = {1,        3,        9,        27,      81,
                           243,      729,      2187,     6561,    19683,
                           59049,    177147,   531441,   1594323, 4782969,
                           14348907, 43046721, 129140163};

#define PRIME_WIN_SIZE_POWER 129140163
#define MINSIZE 1024

static uint64_t hash_func(unsigned char *input, unsigned int pos) {
    uint64_t hash = 0;
    for (int i = 0; i < WIN_SIZE; i++)
        hash += (uint64_t)input[pos + WIN_SIZE - 1 - i] * pow_primes[i + 1];
    return hash;
}

void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect) {

	unsigned int prev = 0;

    uint64_t hash = hash_func(buff, 0);

    unsigned int i = MINSIZE;

    vect.push_back(0);

    while (i < buff_size) {
        if (((hash & MODULUS_MASK) == TARGET) || (((i - prev)) == CHUNK_SIZE)) {
#ifdef CDC_DEBUG
            cout << i << endl;
#endif
            vect.push_back(i);
            prev = i;
            i += MINSIZE;
            //hash = 0;
        }
        hash = (hash * PRIME) - ((uint64_t)buff[i] * PRIME_WIN_SIZE_POWER) +
               ((uint64_t)buff[i + WIN_SIZE] * PRIME);
        i++;
    }

    vect.push_back(buff_size);
#ifdef CDC_DEBUG
    cout << buff_size - 1 << endl;
#endif
}
