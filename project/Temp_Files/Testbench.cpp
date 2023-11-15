#include <iostream>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <stdlib.h>
#include "../Encoder/common.h"

#define FILE_SIZE 4096

using namespace std;

// "Golden" functions to check correctness
std::vector<int> encoding(std::string s1)
{
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
        }
        else {
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
int main()
{
    FILE *fptr = fopen("../../../../Text_Files/LittlePrince.txt", "r");
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

    if (packet_len != output_code.size()) {
    	cout << "FAILURE MISMATCHED PACKET LENGTH!!" << endl;
    	cout << packet_len << "|" << output_code.size() << endl;
    }

    for (int i = 0; i < output_code.size(); i++) {
    	if (output_code[i] != lzw_codes[i]) {
    		cout << "FAILURE!!" << endl;
    		cout << output_code[i] << "|" << lzw_codes[i] << " at i = " << i << endl;
    	}
    }

    cout << "Associate Memory count : " << fill << endl;

	free(file_data);
    return 0;
}
