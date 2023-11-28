#include "../Encoder/common.h"
#include <iostream>
#include <stdlib.h>
#include <unordered_map>
#include <vector>

#define FILE_SIZE 4096

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
    // FILE *fptr = fopen("../../../../Text_Files/LittlePrince.txt", "r");
    // FILE *fptr = fopen("./ruturajn.tgz", "r");
    // FILE *fptr = fopen("../Text_Files/LittlePrince.txt", "r");
    FILE *fptr = fopen("/home1/r/ruturajn/Downloads/embedded_c1.JPG", "rb");
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
    fseek(fptr, file_sz, 0);

    size_t bytes_read = fread(file_data, 1, file_sz, fptr);

    if (bytes_read != file_sz)
        printf("Unable to read file contents");

    file_data[file_sz] = '\0';
    fclose(fptr);
    std::string s;
    char *temp = (char *)file_data;
    int count = 0;

    while (count++ < file_sz) {
        s += *temp;
        temp += 1;
    }

    uint32_t lzw_codes[8192];
    // uint32_t packet_len = 0;
    // unsigned int fill = 0;
    // uint8_t failure = 0;
    LZWData data;
    data.start_idx = 0;
    data.end_idx = file_sz;
    lzw(file_data, lzw_codes, &data);

    std::vector<int> output_code = encoding(s);
    cout << data.out_packet_length << "|" << output_code.size() << endl;

    if (data.failure) {
        cout << "TEST FAILED!!" << endl;
        cout << "FAILED TO INSERT INTO ASSOC MEM!!\n";
        exit(EXIT_FAILURE);
    }

    if (data.out_packet_length != output_code.size()) {
        cout << "TEST FAILED!!" << endl;
        cout << "FAILURE MISMATCHED PACKET LENGTH!!" << endl;
        cout << data.out_packet_length << "|" << output_code.size() << endl;
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < output_code.size(); i++) {
        if (output_code[i] != lzw_codes[i]) {
            cout << "FAILURE!!" << endl;
            cout << output_code[i] << "|" << lzw_codes[i] << " at i = " << i
                 << endl;
        }
    }

    cout << "TEST PASSED!!" << endl;
    cout << "Associate Memory count : " << data.assoc_mem_count << endl;

    free(file_data);
    return 0;
}
