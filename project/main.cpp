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

    cdc(input, NUM_PACKETS * (NUM_ELEMENTS + HEADER), vect);

    for (int i = 0; i < vect.size() - 1; i++) {
        uint16_t *out_packet = (uint16_t *)calloc(MAX_CHUNK_SIZE, sizeof(uint16_t));
        CHECK_MALLOC(out_packet, "Unable to allocate memory for LZW codes");
        sha_fingerprint = sha_256(input, vect[i], vect[i+1]);
        dedup(input, vect[i], vect[i+1], sha_fingerprint, out_packet);
        free(out_packet);
    }
}

int main(int argc, char* argv[]) {
	stopwatch ethernet_timer;
	// unsigned char* input[NUM_PACKETS];
	int writer = 0;
	int done = 0;
	int length = 0;
	int count = 0;
	ESE532_Server server;

	// default is 2k
	int blocksize = BLOCKSIZE;

	// set blocksize if decalred through command line
	handle_input(argc, argv, &blocksize);

    unsigned char *input = (unsigned char *)calloc(NUM_PACKETS * (NUM_ELEMENTS + HEADER), sizeof(unsigned char));
    CHECK_MALLOC(input, "Unable to allocate memory for input buffer");

	server.setup_server(blocksize);

	writer = pipe_depth;

	//last message
	while (!done) {
		// reset ring buffer

        /**********************************************************************
         * Perform the actual computation here. The idea is to maintain a buffer,
         * that will hold multiple packets, so that CDC can chunk at appropriate
         * boundaries. Call the compression pipeline function after the buffer is
         * completely filled.
         *********************************************************************/
		if (writer == NUM_PACKETS * (NUM_ELEMENTS + HEADER)) {
			writer = 0;
            compression_pipeline(input);
		}

		ethernet_timer.start();
		server.get_packet(input + writer);
		ethernet_timer.stop();

		count++;

		// get packet
		unsigned char* buffer = input + writer;

		// decode
		done = buffer[1] & DONE_BIT_L;
		length = buffer[0] | (buffer[1] << 8);
		length &= ~DONE_BIT_H;

		offset += length;
		writer += (NUM_ELEMENTS + HEADER);
	}

    free(input);

	std::cout << "--------------- Key Throughputs ---------------" << std::endl;
	float ethernet_latency = ethernet_timer.latency() / 1000.0;
	// float input_throughput = (bytes_written * 8 / 1000000.0) / ethernet_latency; // Mb/s
	// std::cout << "Input Throughput to Encoder: " << input_throughput << " Mb/s."
	// 		<< " (Latency: " << ethernet_latency << "s)." << std::endl;

	return 0;
}
