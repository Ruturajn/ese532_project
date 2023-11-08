#include "common.h"
#include <math.h>

using namespace std;

static uint64_t hash_func(unsigned char *input, unsigned int pos) {
        uint64_t hash = 0;
        for (int i = 0; i < WIN_SIZE; i++)
                hash += (uint64_t)input[pos+WIN_SIZE-1-i]*pow(PRIME, i+1);
        return hash;
}

void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect) {

    uint64_t hash = hash_func(buff, 0);

//     printf("\nInside CDC-%d\n", buff_size);

//     printf("Inside Buffer\n");
//     for(int d = 0; d < 20 ; d++){
//         printf("%c",buff[d]);
//     }

    for (unsigned int i = 0; i < buff_size; i++) {
        if ((hash % 4096) == TARGET || ((i % 4096) == TARGET)) {
                cout << i << endl;
                vect.push_back(i);
        }
        hash = (hash * PRIME) -
               ((uint64_t)buff[i] * pow(PRIME, WIN_SIZE + 1)) +
               ((uint64_t)buff[i + WIN_SIZE] * PRIME);
    }

    vect.push_back(buff_size-1);
}



// trying akhil

// static uint64_t hash_func(unsigned char *input, unsigned int pos) {
//         uint64_t hash = 0;
//         for (int i = 0; i < WIN_SIZE; i++)
//                 hash += int(input[pos+WIN_SIZE-1-i])*pow(PRIME, i+1);
//         return hash;
// }

// void cdc(unsigned char *buff, unsigned int buff_size, vector<uint32_t> &vect) {


//     uint64_t hash = hash_func(buff, WIN_SIZE);

//     for (unsigned int i = WIN_SIZE; i < buff_size - WIN_SIZE; i++) {
//         if ((hash % 4096) == TARGET) {
//                 cout << i << endl;
//                 vect.push_back(i);
//         }
//         hash = (hash * PRIME) -
//                (buff[i] * pow(PRIME, WIN_SIZE + 1)) +
//                (buff[i + WIN_SIZE] * PRIME);
//     }
// }
