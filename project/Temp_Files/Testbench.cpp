#include "../Encoder/common.h"
#include <iostream>
#include <stdlib.h>
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

static unsigned char *create_packet_sw(uint32_t out_packet_length,
                                    vector<int> out_packet, uint32_t packet_len) {
    unsigned char *data =
        (unsigned char *)calloc(packet_len, sizeof(unsigned char));
    CHECK_MALLOC(data, "Unable to allocate memory for new data packet");

    uint32_t data_idx = 0;
    uint16_t current_val = 0;
    int bits_left = 0;
    int current_val_bits_left = 0;

    for (uint32_t i = 0; i < out_packet_length; i++) {
        current_val = out_packet[i];
        current_val_bits_left = CODE_LENGTH;

        if (bits_left == 0 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] = (current_val >> 4) & 0xFF;
            bits_left = 0;
            current_val_bits_left = 4;
            data_idx += 1;
        }

        if (bits_left == 0 && current_val_bits_left == 4) {
            if (data_idx < packet_len) {
                data[data_idx] = (current_val & 0x0F) << 4;
                bits_left = 4;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }

        if (bits_left == 4 && current_val_bits_left == CODE_LENGTH) {
            data[data_idx] |= ((current_val >> 8) & 0x0F);
            bits_left = 0;
            data_idx += 1;
            current_val_bits_left = 8;
        }

        if (bits_left == 0 && current_val_bits_left == 8) {
            if (data_idx < packet_len) {
                data[data_idx] = ((current_val)&0xFF);
                bits_left = 0;
                data_idx += 1;
                current_val_bits_left = 0;
                continue;
            } else
                break;
        }
    }
    return data;
}

//****************************************************************************************************************
int main() {
    // FILE *fptr = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/Franklin.txt", "r");
    // FILE *fptr = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/lotr.txt", "r");
    // FILE *fptr = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/imaggeee.jpg", "r");
    // FILE *fptr = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/IMAGE_390.jpg", "r");
    // FILE *fptr = fopen("./ruturajn.tgz", "r");
    FILE *fptr = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/LittlePrince.txt", "r");
    // FILE *fptr = fopen("/home1/r/ruturajn/Downloads/embedded_h5.JPG", "rb");
	// FILE *fptr = fopen("/home1/r/ruturajn/Downloads/ESE5070_Assignment3_ruturajn-1.pdf", "rb");
    // FILE *fptr = fopen("/home1/r/ruturajn/Downloads/FiraCode.zip", "rb");
    // FILE *fptr = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/encoder.xo", "r");
    if (fptr == NULL) {
        printf("Unable to open file!\n");
        exit(EXIT_FAILURE);
    }

	fseek(fptr, 0, SEEK_END); // seek to end of file
	int64_t file_sz = ftell(fptr); // get current file pointer
	fseek(fptr, 0, SEEK_SET); // seek back to beginning of file

    // file_sz = FILE_SIZE <= file_sz ? FILE_SIZE : file_sz;

    FILE *fptr_write_2 = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/comp_gen.bin", "wb");
    if (fptr_write_2 == NULL) {
        printf("Unable to open file to write contents for kernel!\n");
        exit(EXIT_FAILURE);
    }

    FILE *fptr_write = fopen("/home1/r/ruturajn/ESE532/ese532_project/project/Text_Files/comp.bin", "wb");
    if (fptr_write == NULL) {
        printf("Unable to open file to write contents!\n");
        exit(EXIT_FAILURE);
    }

    unsigned char file_data[16384];
    uint32_t chunk_indices[20];
    uint32_t lzw_codes[40960];
    unsigned char data[40960 * 4] = {0};
    unsigned int count = 0;

	while (file_sz > 0) {

        size_t bytes_read = fread(file_data, 1, 16384, fptr);

        if (file_sz >= 16384 && bytes_read != 16384)
            printf("Unable to read file contents");

//        if (file_sz < 16384)
//        	file_data[file_sz] = '\0';

        vector<uint32_t> vect;

        if (file_sz < 16384)
        	fast_cdc(file_data, (unsigned int)file_sz, 4096, vect);
        	//cdc(file_data, (unsigned int)file_sz, vect);
        else
        	fast_cdc(file_data, (unsigned int)16384, 4096, vect);
        	//cdc(file_data, (unsigned int)16384, vect);

        chunk_indices[0] = vect.size();

        std::copy(vect.begin(), vect.end(), chunk_indices + 1);

        uint32_t out_packet_lengths[20];
//        int32_t dedup_out[20] = {-1, 22, -1, -1, 44, -1, -1, -1,
//        		-1, 22, -1, -1, 44, -1, -1, -1,
//	    		-1, 22, -1, -1};
        int64_t dedup_out[20] = {-1, -1, -1, -1, -1,
                                 -1, -1, -1, -1, -1,
                                 -1, -1, -1, -1, -1,
                                 -1, -1, -1, -1, -1};

        lzw(file_data, data, lzw_codes, chunk_indices, out_packet_lengths, dedup_out);

        uint32_t *lzw_codes_ptr = lzw_codes;

	    if (out_packet_lengths[0] & 0x1) {
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
                exit(EXIT_FAILURE);
            }

            for (int j = 0; j < output_code.size(); j++) {
                if (output_code[j] != lzw_codes_ptr[j]) {
                    cout << "FAILURE!!" << endl;
                    cout << output_code[j] << "|" << lzw_codes_ptr[j]
                         << " at j = " << j << " and i = " << i << endl;
                }
            }

            if (dedup_out[i - 1] == -1) {
                packet_len = ((output_code.size() * 12) / 8);
                packet_len =
                    (output_code.size() % 2 != 0) ? packet_len + 1 : packet_len;

                unsigned char *data_packet =
                    create_packet_sw(output_code.size(), output_code, packet_len);

                header = packet_len << 1;
                final_data.push_back({{header, packet_len}, data_packet});
            } else {
                header = (dedup_out[i - 1] << 1) | 1;
                final_data.push_back({{header, -1}, NULL});
            }

            lzw_codes_ptr += out_packet_lengths[i];
        }

        for (auto it : final_data) {
            if (it.second != NULL) {
                fwrite(&it.first.first, sizeof(uint32_t), 1, fptr_write);
                fwrite(it.second, sizeof(unsigned char), it.first.second,
                       fptr_write);
                free(it.second);
            } else {
                fwrite(&it.first.first, sizeof(uint32_t), 1, fptr_write);
            }
        }


        fwrite(data, sizeof(unsigned char) * (out_packet_lengths[0] >> 1), 1, fptr_write_2);

        file_sz -= 16384;

        cout << "Iteration : " << count << endl;
        count++;
	}

    fclose(fptr);
    fclose(fptr_write);
    fclose(fptr_write_2);

    cout << "TEST PASSED!!" << endl;
    return 0;
}
