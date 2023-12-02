#include "../Server/encoder.h"
#include "../Server/server.h"
#include "../Server/stopwatch.h"
#include "common.h"
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM_PACKETS 16
#define pipe_depth 4
#define DONE_BIT_L (1 << 7)
#define DONE_BIT_H (1 << 15)

using namespace std;

uint64_t dedup_bytes = 0;
uint64_t lzw_bytes = 0;

void handle_input(int argc, char *argv[], int *blocksize, char **filename) {
    int x;
    extern char *optarg;

    while ((x = getopt(argc, argv, ":b:f:")) != -1) {
        switch (x) {
        case 'b':
            *blocksize = atoi(optarg);
            printf("blocksize is set to %d optarg\n", *blocksize);
            break;
        case 'f':
            *filename = optarg;
            printf("filename is %s\n", *filename);
            break;
        case ':':
            printf("-%c without parameter\n", optopt);
            break;
        }
    }
}

static unsigned char *create_packet(int32_t chunk_idx,
                                    uint32_t out_packet_length,
                                    uint32_t *out_packet, uint32_t packet_len) {
    unsigned char *data =
        (unsigned char *)calloc(packet_len, sizeof(unsigned char));
    CHECK_MALLOC(data, "Unable to allocate memory for new data packet");

    int data_idx = 0;
    uint16_t current_val = 0;
    int bits_left = 0;
    int current_val_bits_left = 0;

    for (int i = 0; i < out_packet_length; i++) {
        current_val = out_packet[i];
        current_val_bits_left = CODE_LENGTH;

        if (bits_left == 0 && current_val_bits_left == CODE_LENGTH) {
            // 0000| 0101 0101 0101
            // 0000| 0000 0000 1111
            // 0000| 0000 0000 0101
            // 0000| 0000 0101 0000
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

static void compression_pipeline(unsigned char *input, int length_sum,
                                 FILE *fptr_write) {
    vector<uint32_t> vect;
    string sha_fingerprint;
    int64_t chunk_idx = 0;
    uint32_t out_packet_length = 0;
    uint32_t *out_packet = NULL;
    uint32_t packet_len = 0;
    uint32_t header = 0;
    uint8_t failure = 0;
    unsigned int assoc_mem = 0;

    cdc(input, length_sum, vect);

    for (int i = 0; i < vect.size() - 1; i++) {
        sha_fingerprint = sha_256(input, vect[i], vect[i + 1]);
        chunk_idx = dedup(sha_fingerprint);

#ifdef MAIN_DEBUG
        printf("CHUNK IDX: %ld\n", chunk_idx);
#endif
        printf("CHUNK IDX: %ld\n", chunk_idx);

        if (chunk_idx == -1) {
#ifdef MAIN_DEBUG
            printf("UNIQUE CHUNK\n");
#endif
            out_packet = (uint32_t *)calloc(MAX_CHUNK_SIZE, sizeof(uint32_t));
            CHECK_MALLOC(out_packet, "Unable to allocate memory for LZW codes");
            lzw(input, vect[i], vect[i + 1], out_packet, &out_packet_length,
                &failure, &assoc_mem);

            cout << "Associative Mem count is : " << assoc_mem << endl;

            if (failure) {
                printf("FAILED TO INSERT INTO ASSOC MEM!!\n");
                exit(EXIT_FAILURE);
            }

#ifdef MAIN_DEBUG
            printf("LZW CODES\n");
            for (int i = 0; i < out_packet_length; i++)
                printf("%d ", out_packet[i]);

            putchar('\n');
#endif

            packet_len = ((out_packet_length * 12) / 8);
            packet_len = (chunk_idx == -1 && (out_packet_length % 2 != 0))
                             ? packet_len + 1
                             : packet_len;

            /* printf("CODES ARRAY LENGTH : %d\n", out_packet_length); */

            unsigned char *data_packet = create_packet(
                chunk_idx, out_packet_length, out_packet, packet_len);

            header = packet_len << 1;
            fwrite(&header, sizeof(uint32_t), 1, fptr_write);
            lzw_bytes += 4;

            /* printf("DATA: %x ", header); */

            /* for (int i = 0; i < packet_len; i++) */
            /*     printf("%x", data_packet[i]); */

            /* putchar('\n'); */

            // Write data packet in file.
            // Send out the data packet.
            // | 31:1  [compressed chunk length in bytes or chunk index] | 0 | 9
            // byte data |
            fwrite(data_packet, sizeof(unsigned char), packet_len, fptr_write);
            lzw_bytes += packet_len;
            /* printf("PACKET LENGTH : %d\n", packet_len); */

            free(data_packet);
            free(out_packet);
        } else {
            header = (chunk_idx << 1) | 1;
            /* printf("DATA: %x\n", header); */
            fwrite(&header, sizeof(uint32_t), 1, fptr_write);
            dedup_bytes += 4;
            /* printf("PACKET LENGTH : %d\n", 4); */
        }
    }
}

int main(int argc, char *argv[]) {

#ifdef SHA_TEST
    unsigned char text1[] = {"This is November"};
    string hash_val =
        "d3c5566f1395fac4fdd9d3be948e899b26834ad349b9aded0d8b7e030970f760";
    string out = sha_256(text1, 0, strlen((const char *)text1));
    string out1 = sha_256(text1, 0, strlen((const char *)text1));

    if (out1 == out)
        cout << "TEST PASSED" << endl;
    else {
        cout << out << endl;
        cout << "TEST FAILED!!" << endl;
    }
#endif

    stopwatch ethernet_timer;
    stopwatch compression_timer;
    unsigned char *input[NUM_PACKETS];
    int writer = 0;
    int done = 0;
    int length = -1;
    uint64_t offset = 0;
    int sum = 0;
    // ESE532_Server server;

    FILE *fptr = fopen("./Text_Files/Franklin.txt", "r");
    // FILE *fptr = fopen("./Temp_Files/ruturajn.tgz", "r");
    // FILE *fptr = fopen("/home1/r/ruturajn/Downloads/embedded_c1.JPG", "rb");
    if (fptr == NULL) {
        printf("Error reading file!!\n");
        exit(EXIT_FAILURE);
    }

    // default is 1k
    int blocksize = BLOCKSIZE;
    char *file = strdup("compressed_file.bin");

    // set blocksize if decalred through command line
    handle_input(argc, argv, &blocksize, &file);

    FILE *fptr_write = fopen(file, "wb");
    if (fptr_write == NULL) {
        printf("Error creating file for compressed output!!\n");
        exit(EXIT_FAILURE);
    }

    unsigned char *pipeline_buffer =
        (unsigned char *)calloc(NUM_PACKETS * blocksize, sizeof(unsigned char));
    CHECK_MALLOC(pipeline_buffer,
                 "Unable to allocate memory for pipeline buffer");

    for (int i = 0; i < (NUM_PACKETS); i++) {
        input[i] = (unsigned char *)calloc((blocksize + HEADER),
                                           sizeof(unsigned char));
        CHECK_MALLOC(input, "Unable to allocate memory for input buffer");
    }

    // server.setup_server(blocksize);

    writer = 0;

    // last message
    //  while (!done) {
    while (length != 0) {
        // reset ring buffer

        // ethernet_timer.start();
        // server.get_packet(input[writer]);
        // ethernet_timer.stop();

        length = fread(input[writer], sizeof(unsigned char), 1024, fptr);

        // get packet
        unsigned char *buffer = input[writer];

        // decode
        // done = buffer[1] & DONE_BIT_L;
        // length = buffer[0] | (buffer[1] << 8);
        // length &= ~DONE_BIT_H;

        offset += length;

#ifdef MAIN_DEBUG
        printf("Length - %d\n", length);
        printf("OFFSET - %d\n", writer * 1024);
#endif

        sum += length;

        // Perform the actual computation here. The idea is to maintain a
        // buffer, that will hold multiple packets, so that CDC can chunklength)
        // at appropriate boundaries. Call the compression pipeline function
        // after the buffer is completely filled.
        if (length != 0)
            memcpy(pipeline_buffer + (writer * 1024), input[writer], length);
        // memcpy(pipeline_buffer + (writer * 1024), input[writer] + 2, length);

        // if (writer == (NUM_PACKETS - 1) || (length < 1024 && length > 0) ||
        // done == 1) {
        if (writer == (NUM_PACKETS - 1) || (length < 1024 && length > 0)) {
#ifdef MAIN_DEBUG
            printf("BUFFER\n");
            for (int d = 0; d < sum; d++) {
                // printf("%d\n", d);
                printf("%c", pipeline_buffer[d]);
            }
            printf("\nSum when called - %d\n", sum);
#endif

            compression_timer.start();
            compression_pipeline(pipeline_buffer, sum, fptr_write);
            compression_timer.stop();
            writer = 0;
            sum = 0;
        } else
            writer += 1;
    }

    free(pipeline_buffer);

    for (int i = 0; i < (NUM_PACKETS); i++)
        free(input[i]);

    fclose(fptr);
    fclose(fptr_write);

    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    float compression_latency = compression_timer.latency() / 1000.0;
    float throughput = (offset * 8 / 1000000.0) / compression_latency; // Mb/s
    // std::cout << "Throughput of the Encoder: " << throughput << " Mb/s."
    //         << " (Ethernet Latency: " << ethernet_latency << "s)." <<
    //         std::endl;
    cout << "Ethernet Latency: " << ethernet_latency << "s." << endl;
    cout << "Bytes Received: " << offset << "B." << endl;
    cout << "Latency for Compression: " << compression_latency << "s." << endl;
    cout << "Application Throughput: " << throughput << "Mb/s." << endl;
    cout << "Bytes Contributed by Deduplication: " << dedup_bytes << "B."
         << endl;
    cout << "Bytes Contributed by LZW: " << lzw_bytes << "B." << endl;

    return 0;
}
