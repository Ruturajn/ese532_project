#include "../Encoder/common.h"
#include <iostream>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#define FILE_SIZE 16383

using namespace std;

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

//****************************************************************************************************************
int main() {
    FILE *fptr = fopen("../../../../Text_Files/Franklin.txt", "r");
    // FILE *fptr = fopen("./ruturajn.tgz", "r");
    // FILE *fptr = fopen("../Text_Files/LittlePrince.txt", "r");
    // FILE *fptr = fopen("/home1/r/ruturajn/Downloads/embedded_c1.JPG", "rb");
    if (fptr == NULL) {
        printf("Unable to open file!\n");
        exit(EXIT_FAILURE);
    }

    size_t file_sz = FILE_SIZE;

    unsigned char file_data[16384];

    size_t bytes_read = fread(file_data, 1, file_sz, fptr);

    if (bytes_read != file_sz)
        printf("Unable to read file contents");

    file_data[file_sz] = '\0';
    fclose(fptr);

    uint32_t lzw_codes[40960];
    uint8_t failure = 0;

    uint32_t chunk_indices[4096];

    chunk_indices[0] = 19;
    chunk_indices[1] = 0;
    chunk_indices[2] = 123;
    chunk_indices[3] = 601;
    chunk_indices[4] = 719;
    chunk_indices[5] = 890;
    chunk_indices[6] = 1621;
    chunk_indices[7] = 2434;
    chunk_indices[8] = 5146;
    chunk_indices[9] = 6104;
    chunk_indices[10] = 6321;
    chunk_indices[11] = 6768;
    chunk_indices[12] = 7664;
    chunk_indices[13] = 8310;
    chunk_indices[14] = 8386;
    chunk_indices[15] = 9720;
    chunk_indices[16] = 9953;
    chunk_indices[17] = 10516;
    chunk_indices[18] = 10831;
    chunk_indices[19] = 11341;

    uint32_t out_packet_lengths[8192];

    lzw(file_data, lzw_codes, chunk_indices, out_packet_lengths);

    uint32_t *lzw_codes_ptr = lzw_codes;

    //    if (out_packet_lengths[0]) {
    //        cout << "TEST FAILED!!" << endl;
    //        cout << "FAILED TO INSERT INTO ASSOC MEM!!\n";
    //        exit(EXIT_FAILURE);
    //    }

    for (int i = 1; i <= chunk_indices[0] - 1; i++) {
        std::string s;
        char *temp = (char *)file_data + chunk_indices[i];
        int count = chunk_indices[i];

        while (count++ < chunk_indices[i + 1]) {
            s += *temp;
            temp += 1;
        }

        std::vector<int> output_code = encoding(s);

        if (out_packet_lengths[i] != output_code.size()) {
            cout << "TEST FAILED!!" << endl;
            cout << "FAILURE MISMATCHED PACKET LENGTH!!" << endl;
            cout << out_packet_lengths[i] << "|" << output_code.size()
                 << "at i = " << i << endl;
            exit(EXIT_FAILURE);
        }

        for (int j = 0; j < output_code.size(); j++) {
            if (output_code[j] != lzw_codes_ptr[j]) {
                cout << "FAILURE!!" << endl;
                cout << output_code[i] << "|" << lzw_codes_ptr[j]
                     << " at j = " << j << endl;
            }
        }

        lzw_codes_ptr += out_packet_lengths[i];
    }

    cout << "TEST PASSED!!" << endl;
    return 0;
}
