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

#define NUM_PACKETS 8
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

static void compression_pipeline(unsigned char *input) {
    vector<uint32_t> vect;
    string sha_fingerprint;
    uint32_t chunk_idx = 0, out_packet_length = 0;
    uint16_t *out_packet = NULL;
    uint32_t packet_len = 0;

    cdc(input, NUM_PACKETS * (NUM_ELEMENTS + HEADER), vect);

    // printf("VECTOR ELEMENTS\n");
    // for (auto elem: vect)
    //     cout << elem << " " << endl;
    // cout << "VECTOR ELEMENTS END\n";

    for (int i = 0; i < vect.size() - 1; i++) {
        sha_fingerprint = sha_256(input, vect[i], vect[i+1]);
        chunk_idx = dedup(sha_fingerprint);

        if (chunk_idx == -1) {
            out_packet = (uint16_t *)calloc(MAX_CHUNK_SIZE, sizeof(uint16_t));
            CHECK_MALLOC(out_packet, "Unable to allocate memory for LZW codes");
            lzw(input, vect[i], vect[i+1], out_packet, &out_packet_length);
        }

        // Send out the data packet.
        // | 31:1  [compressed chunk length in bytes or chunk index] | 0 | 9 byte data |
        packet_len = (4 + ((out_packet_length * 13) / 8)) + 1;
        unsigned char *data_packet = (unsigned char *)calloc(packet_len, sizeof(unsigned char));
        CHECK_MALLOC(data_packet, "Unable to allocate memory for new data packet");

        if (chunk_idx != -1) {
            // Configre the Header.
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

        // for (int i = 0; i < packet_len; i++)
        //     printf("%hhX ", data_packet[i]);

        // printf("\n");

        if (chunk_idx == -1)
            free(out_packet);

        free(data_packet);
    }
}

int main(int argc, char* argv[]) {

    /*
       unsigned char text1[] = {"This is November"};
       string hash_val = "d3c5566f1395fac4fdd9d3be948e899b26834ad349b9aded0d8b7e030970f760";
       string out = sha_256(text1, 0, strlen((const char *)text1));
       string out1 = sha_256(text1, 0, strlen((const char *)text1));

       if (out1 == out)
       cout << "TEST PASSED" << endl;
       else {
       cout << out << endl;
       cout << "TEST FAILED!!" << endl;
       }*/

    stopwatch ethernet_timer;
    unsigned char* input[NUM_PACKETS * 2];
    int writer = 0;
    int done = 0;
    int length = 0;
    int count = 0;
    ESE532_Server server;

    // default is 2k
    int blocksize = BLOCKSIZE;

    // set blocksize if decalred through command line
    handle_input(argc, argv, &blocksize);

    unsigned char *pipeline_buffer = (unsigned char *)calloc((NUM_PACKETS * 2) * (NUM_ELEMENTS + HEADER), sizeof(unsigned char));
    CHECK_MALLOC(pipeline_buffer, "Unable to allocate memory for pipeline buffer");

    for (int i = 0; i < (NUM_PACKETS * 2); i++) {
        input[i] = (unsigned char *)calloc((NUM_ELEMENTS + HEADER), sizeof(unsigned char));
        CHECK_MALLOC(input, "Unable to allocate memory for input buffer");
    }

    server.setup_server(blocksize);

    writer = pipe_depth;

    //last message
    while (!done) {
        // reset ring buffer

        // Perform the actual computation here. The idea is to maintain a buffer,
        // that will hold multiple packets, so that CDC can chunk at appropriate
        // boundaries. Call the compression pipeline function after the buffer is
        // completely filled.
        if (writer == (2 * NUM_PACKETS)) {
            writer = 0;
            for (int i = 0; i < (2 * NUM_PACKETS); i++)
                memcpy(pipeline_buffer + (i * (NUM_ELEMENTS + HEADER)), input[i], (NUM_ELEMENTS + HEADER));
            compression_pipeline(pipeline_buffer);
        }

        ethernet_timer.start();
        server.get_packet(input[writer]);
        ethernet_timer.stop();

        count++;

        // get packet
        unsigned char* buffer = input[writer];

        // decode
        done = buffer[1] & DONE_BIT_L;
        length = buffer[0] | (buffer[1] << 8);
        length &= ~DONE_BIT_H;

        writer += 1;
    }

    free(pipeline_buffer);

    for (int i = 0; i < (2 * NUM_PACKETS); i++)
        free(input[i]);

    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    // float input_throughput = (bytes_written * 8 / 1000000.0) / ethernet_latency; // Mb/s
    // std::cout << "Input Throughput to Encoder: " << input_throughput << " Mb/s."
    // 		<< " (Latency: " << ethernet_latency << "s)." << std::endl;

    return 0;
}
