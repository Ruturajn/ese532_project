#include "../Encoder/common.h"
#include <iostream>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#define FILE_SIZE 14247

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
    FILE *fptr = fopen("../../../../Text_Files/LittlePrince.txt", "r");
    // FILE *fptr = fopen("./ruturajn.tgz", "r");
    // FILE *fptr = fopen("../Text_Files/LittlePrince.txt", "r");
    // FILE *fptr = fopen("/home1/r/ruturajn/Downloads/embedded_c1.JPG", "rb");
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
    // fseek(fptr, file_sz, 0);

    size_t bytes_read = fread(file_data, 1, file_sz, fptr);

    if (bytes_read != file_sz)
        printf("Unable to read file contents");

    file_data[file_sz] = '\0';
    fclose(fptr);

    uint32_t lzw_codes[16384];
    // uint32_t packet_len = 0;
    // unsigned int fill = 0;
    uint8_t failure = 0;

    uint32_t chunk_indices[6] = {5, 0, 4096, 8192, 12288, 14247};
    uint32_t out_packet_lengths[5] = {0};

    lzw(file_data, lzw_codes, chunk_indices, out_packet_lengths);

    uint32_t *lzw_codes_ptr = &lzw_codes[0];

    if (out_packet_lengths[0]) {
        cout << "TEST FAILED!!" << endl;
        cout << "FAILED TO INSERT INTO ASSOC MEM!!\n";
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i <= 4; i++) {
        std::string s;
        char *temp = (char *)file_data + chunk_indices[i];
        int count = chunk_indices[i];

        while (count++ < chunk_indices[i+1]) {
            s += *temp;
            temp += 1;
        }

        std::vector<int> output_code = encoding(s);

        if (out_packet_lengths[i] != output_code.size()) {
            cout << "TEST FAILED!!" << endl;
            cout << "FAILURE MISMATCHED PACKET LENGTH!!" << endl;
            cout << out_packet_lengths[i] << "|" << output_code.size() << endl;
            exit(EXIT_FAILURE);
        }

        for (int j = 0; j < output_code.size(); j++) {
            if (output_code[j] != lzw_codes_ptr[j]) {
                cout << "FAILURE!!" << endl;
                cout << output_code[i] << "|" << lzw_codes_ptr[j] << " at j = " << j
                     << endl;
            }
        }

        lzw_codes_ptr += out_packet_lengths[i];
    }

    cout << "TEST PASSED!!" << endl;
    free(file_data);
    return 0;
}
