#include "../Encoder/common.h"
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unordered_map>
#include <vector>

#define FILE_SIZE 16383
#define MAX_LZW_CODES (4096 * 20)

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
            //             std::cout << p << "\t" << table[p] << "\t\t"
            //                  << p + c << "\t" << code << std::endl;
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

//**********************************************************************
int main() {
    // FILE *fptr =
    // fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/Franklin.txt",
    // "r"); FILE *fptr =
    // fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/lotr.txt",
    // "r"); FILE *fptr =
    // fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/imaggeee.jpg",
    // "r"); FILE *fptr =
    // fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/IMAGE_390.jpg",
    // "r"); FILE *fptr = fopen("./ruturajn.tgz", "r");
    FILE *fptr = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/"
                       "Text_Files/LittlePrince.txt",
                       "r");
    // FILE *fptr =
    // fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/small_prince.txt",
    // "r"); FILE *fptr = fopen("/home1/r/ruturajn/Downloads/embedded_h5.JPG",
    // "rb"); FILE *fptr =
    // fopen("/home1/r/ruturajn/Downloads/ESE5070_Assignment3_ruturajn-1.pdf",
    // "rb");
    // FILE *fptr = fopen("/home1/r/ruturajn/Downloads/FiraCode.zip", "rb");
    // FILE *fptr =
    // fopen("/home1/r/ruturajn/ESE532/ese532_project/project/encoder.xo", "r");
    if (fptr == NULL) {
        printf("Unable to open file!\n");
        exit(EXIT_FAILURE);
    }

    fseek(fptr, 0, SEEK_END);      // seek to end of file
    int64_t file_sz = ftell(fptr); // get current file pointer
    fseek(fptr, 0, SEEK_SET);      // seek back to beginning of file

    unsigned char file_data[16384];
    uint32_t chunk_indices[20];
    uint32_t *lzw_codes = (uint32_t *)calloc(40960, sizeof(uint32_t));
    memset(lzw_codes, 23, 40960 * sizeof(uint32_t));
    if (lzw_codes == NULL) {
        cout << "Unable to allocate memory for lzw codes!" << endl;
        exit(EXIT_FAILURE);
    }
    uint32_t out_packet_lengths[20];
    unsigned int count = 0;
    bool fail_stat = false;

    while (file_sz > 0) {

        size_t bytes_read = fread(file_data, 1, 16384, fptr);

        if (file_sz >= 16384 && bytes_read != 16384)
            printf("Unable to read file contents");

        vector<uint32_t> vect;

        if (file_sz < 16384)
            fast_cdc(file_data, (unsigned int)file_sz, 4096, vect);
        else
            fast_cdc(file_data, (unsigned int)16384, 4096, vect);

        chunk_indices[0] = vect.size();

        std::copy(vect.begin(), vect.end(), chunk_indices + 1);

        lzw(file_data, lzw_codes, chunk_indices, out_packet_lengths);

        uint32_t *lzw_codes_ptr = lzw_codes;

        if (out_packet_lengths[0]) {
            cout << "TEST FAILED!!" << endl;
            cout << "FAILED TO INSERT INTO ASSOC MEM!!\n";
            exit(EXIT_FAILURE);
        }

        uint32_t packet_len = 0;
        uint32_t header = 0;
        vector<pair<pair<int, int>, unsigned char *>> final_data;

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
                fail_stat = true;
                exit(EXIT_FAILURE);
            }

            for (int j = 0; j < output_code.size(); j++) {
                if (output_code[j] != lzw_codes_ptr[j]) {
                    if (!fail_stat)
                        fail_stat = true;
                    cout << "FAILURE!!" << endl;
                    cout << output_code[j] << "|" << lzw_codes_ptr[j]
                         << " at j = " << j << " and i = " << i << endl;
                }
            }

            lzw_codes_ptr += out_packet_lengths[i];
        }

        file_sz -= 16384;

        cout << "Iteration : " << count << endl;
        count++;
    }

    fclose(fptr);

    if (fail_stat)
        cout << "TEST FAILED!!" << endl;
    else
        cout << "TEST PASSED!!" << endl;

    return 0;
}
