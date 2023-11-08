#include "common.h"
#include "Server/encoder.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include "Server/server.h"
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "Server/stopwatch.h"

#define NUM_PACKETS 16
#define pipe_depth 4
#define DONE_BIT_L (1 << 7)
#define DONE_BIT_H (1 << 15)

int offset = 0;
unsigned char* file;

using namespace std;

void handle_input(int argc, char* argv[], int* blocksize) {
    int x;
    extern char *optarg;

    while ((x = getopt(argc, argv, ":b:")) != -1) {
        switch (x) {
            case 'b':
                *blocksize = atoi(optarg);
                printf("blocksize is set to %d optarg\n", *blocksize);
                break;
            case ':':
                printf("-%c without parameter\n", optopt);
                break;
        }
    }
}

static unsigned char *create_packet(uint32_t packet_len, int64_t chunk_idx,
                                    uint32_t out_packet_length, uint16_t *out_packet) {
    // Send out the data packet.
    // | 31:1  [compressed chunk length in bytes or chunk index] | 0 | 9 byte data |
    packet_len = (4 + ((out_packet_length * 13) / 8)) + 1;
    unsigned char *data_packet = (unsigned char *)calloc(packet_len, sizeof(unsigned char));
    CHECK_MALLOC(data_packet, "Unable to allocate memory for new data packet");

    if (chunk_idx != -1) {
        // Configure the Header.
        // Set the 0th bit of byte 4 to signify duplicate chunk.
        // 0x00 00 00 00
        data_packet[0] = ((chunk_idx >> 24) & 0xFF);
        data_packet[1] = (chunk_idx >> 16) & 0xFF;
        data_packet[2] = (chunk_idx >> 8) & 0xFF;
        data_packet[3] = (chunk_idx & 0xFF) | 1;
    } else {
        data_packet[0] = ((out_packet_length >> 24) & 0xFF);
        data_packet[1] = (out_packet_length >> 16) & 0xFF;
        data_packet[2] = (out_packet_length >> 8) & 0xFF;
        data_packet[3] = (out_packet_length & 0xFF);
    }

    if (chunk_idx == -1) {
        for (int i = 4; i < packet_len; i += 2) {
            if ((i - 4) < packet_len) {
                data_packet[i] = (out_packet[i] >> 8) & 0xFF;
                data_packet[i+1] = (out_packet[i] & 0xFF);
            }
        }
    } 
}

static void compression_pipeline(unsigned char *input, int length_sum) {
    vector<uint32_t> vect;
    vector<unsigned char> sha_fingerprint(32);
    int64_t chunk_idx = 0;
    uint32_t out_packet_length = 0;
    uint16_t *out_packet = NULL;
    uint32_t packet_len = 0;

    cdc(input, length_sum, vect);

    for (int i = 0; i < vect.size() - 1; i++) {
        sha_256(input, vect[i], vect[i+1], sha_fingerprint);
        chunk_idx = dedup(sha_fingerprint);

        cout << chunk_idx << endl;

        if (chunk_idx == -1) {
            printf("CALLED\n");
            out_packet = (uint16_t *)calloc(MAX_CHUNK_SIZE, sizeof(uint16_t));
            CHECK_MALLOC(out_packet, "Unable to allocate memory for LZW codes");
            lzw(input, vect[i], vect[i+1], out_packet, &out_packet_length);
        }

        create_packet(packet_len, chunk_idx, out_packet_length, out_packet);

        if (chunk_idx == -1)
            free(out_packet);

        free(data_packet);
    }
}

int main(int argc, char* argv[]) {

#ifdef SHA_TEST
    unsigned char text1[] = {"This is November"};
    string hash_val = "d3c5566f1395fac4fdd9d3be948e899b26834ad349b9aded0d8b7e030970f760";
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
    unsigned char* input[NUM_PACKETS];
    int writer = 0;
    int done = 0;
    int length = -1;
    int count = 0;
    int sum = 0;
    //ESE532_Server server;

    FILE *fptr = fopen("Franklin.txt", "r");
    if (fptr == NULL) {
        printf("Error reading file!!\n");
        exit(EXIT_FAILURE);
    }

    // default is 1k
    int blocksize = BLOCKSIZE;

    // set blocksize if decalred through command line
    handle_input(argc, argv, &blocksize);

    unsigned char *pipeline_buffer = (unsigned char *)calloc(NUM_PACKETS * blocksize, sizeof(unsigned char));
    CHECK_MALLOC(pipeline_buffer, "Unable to allocate memory for pipeline buffer");

    for (int i = 0; i < (NUM_PACKETS); i++) {
        input[i] = (unsigned char *)calloc((blocksize + HEADER), sizeof(unsigned char));
        CHECK_MALLOC(input, "Unable to allocate memory for input buffer");
    }

    // server.setup_server(blocksize);

    //writer = pipe_depth;
    writer = 0;

    //last message
    while (length != 0) {
        // reset ring buffer

        // ethernet_timer.start();
        // server.get_packet(input[writer]);
        // ethernet_timer.stop();

        length = fread(input[writer], sizeof(unsigned char), 1024, fptr);

        // get packet
        unsigned char* buffer = input[writer];

        // decode
        // done = buffer[1] & DONE_BIT_L;
        // length = buffer[0] | (buffer[1] << 8);
        // length &= ~DONE_BIT_H;

#ifdef MAIN_DEBUG
        printf("Length - %d\n",length);
        printf("OFFSET - %d\n",writer * 1024);
#endif

        sum += length;

        // Perform the actual computation here. The idea is to maintain a buffer,
        // that will hold multiple packets, so that CDC can chunklength) at appropriate
        // boundaries. Call the compression pipeline function after the buffer is
        // completely filled.
        if (length != 0)
            memcpy(pipeline_buffer + (writer * 1024), input[writer], length);

        if (writer == (NUM_PACKETS - 1) || (length < 1024 && length > 0)) {
#ifdef MAIN_DEBUG
            printf("BUFFER\n");
            for(int d = 0; d < sum ; d++){
                // printf("%d\n", d);
                printf("%c", pipeline_buffer[d]);
            }
            printf("\nSum when called - %d\n", sum);
#endif

            compression_pipeline(pipeline_buffer, sum);
            writer = 0;
            sum = 0;
        } else
            writer += 1;
    }

    free(pipeline_buffer);

    for (int i = 0; i < (NUM_PACKETS); i++)
        free(input[i]);

    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    // float input_throughput = (bytes_written * 8 / 1000000.0) / ethernet_latency; // Mb/s
    // std::cout << "Input Throughput to Encoder: " << input_throughput << " Mb/s."
    // 		<< " (Latency: " << ethernet_latency << "s)." << std::endl;

    return 0;
}