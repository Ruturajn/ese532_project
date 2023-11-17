#include "../Server/encoder.h"
#include "../Server/server.h"
#include "../Server/stopwatch.h"
#include "Utilities.h"
#include "common.h"
#include <chrono>
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
#include <vector>

#define NUM_PACKETS 2
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

static unsigned char *create_packet(int32_t chunk_idx, uint32_t out_packet_length, uint32_t *out_packet,
                                    uint32_t packet_len) {
    unsigned char *data = (unsigned char *)calloc(packet_len, sizeof(unsigned char));
    CHECK_MALLOC(data, "Unable to allocate memory for new data packet");

    int data_idx = 0;
    uint16_t current_val = 0;
    int bits_left = 0;
    int current_val_bits_left = 0;

    for (int i = 0; i < out_packet_length; i++) {
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

static void compression_pipeline(unsigned char *input, int length_sum, FILE *fptr_write, cl::CommandQueue q,
                                 cl::Kernel kernel_lzw, cl::Buffer lzw_input_buffer, cl::Buffer lzw_output_buffer,
                                 unsigned char *host_input_chunk, uint32_t *host_output_lzw, cl::Buffer out_packet_length_buf,
                                 uint32_t *out_packet_length, cl::Buffer failure_buf, uint8_t *failure,
                                 cl::Buffer assoc_mem_buf, unsigned int *assoc_mem) {

    vector<uint32_t> vect;
    string sha_fingerprint;
    int64_t chunk_idx = 0;
    /* uint32_t out_packet_length = 0; */
    /* uint32_t *out_packet = NULL; */
    uint32_t packet_len = 0;
    uint32_t header = 0;
    /* uint8_t failure = 0; */
    /* unsigned int assoc_mem = 0; */

    // ------------------------------------------------------------------------------------
    // Step 3: Run the kernel
    // ------------------------------------------------------------------------------------

    stopwatch time_cdc;
    stopwatch time_lzw;
    stopwatch time_sha;
    stopwatch time_dedup;
    stopwatch total_time;

    std::vector<cl::Event> write_event(1);
    std::vector<cl::Event> compute_event(1);
    std::vector<cl::Event> done_event(1);

    double total_time_2 = 0;

    total_time.start();
    // RUN CDC
    time_cdc.start();
    cdc(input, length_sum, vect);
    time_cdc.stop();

    memcpy(host_input_chunk, input, sizeof(unsigned char) * length_sum);

    for (int i = 0; i < vect.size() - 1; i++) {

        // RUN SHA
        time_sha.start();
        sha_fingerprint = sha_256(input, vect[i], vect[i + 1]);
        time_sha.stop();

        // RUN DEDUP
        time_dedup.start();
        chunk_idx = dedup(sha_fingerprint);
        time_dedup.stop();

        if (chunk_idx == -1) {
            /* out_packet = (uint32_t *)calloc(MAX_CHUNK_SIZE,
             * sizeof(uint32_t)); */
            /* CHECK_MALLOC(out_packet, "Unable to allocate memory for LZW
             * codes"); */

            time_lzw.start();

            kernel_lzw.setArg(0, lzw_input_buffer);
            kernel_lzw.setArg(1, sizeof(uint32_t), &vect[i]);
            kernel_lzw.setArg(2, sizeof(uint32_t), &vect[i + 1]);
            kernel_lzw.setArg(3, lzw_output_buffer);
            kernel_lzw.setArg(4, out_packet_length_buf);
            kernel_lzw.setArg(5, failure_buf);
            kernel_lzw.setArg(6, assoc_mem_buf);

            q.enqueueMigrateMemObjects({lzw_input_buffer}, 0 /* 0 means from host*/, NULL, &write_event[0]);

            // lzw(input, vect[i], vect[i+1], out_packet, &out_packet_length,
            // &failure, &assoc_mem);

            q.enqueueTask(kernel_lzw, &write_event, &compute_event[0]);

            // Method 2
            compute_event[0].wait();
            total_time_2 += compute_event[0].getProfilingInfo<CL_PROFILING_COMMAND_END>() -
            compute_event[0].getProfilingInfo<CL_PROFILING_COMMAND_START>();

            // q.enqueueTask(kernel_lzw, &write_event, &compute_event[0]);

            q.enqueueMigrateMemObjects({lzw_output_buffer, out_packet_length_buf, failure_buf, assoc_mem_buf}, CL_MIGRATE_MEM_OBJECT_HOST, &compute_event, &done_event[0]);
            // q.enqueueMigrateMemObjects({out_packet_length_buf}, CL_MIGRATE_MEM_OBJECT_HOST, &compute_event,
            //                            &done_event[0]);
            // q.enqueueMigrateMemObjects({failure_buf}, CL_MIGRATE_MEM_OBJECT_HOST, &compute_event, &done_event[0]);
            // q.enqueueMigrateMemObjects({assoc_mem_buf}, CL_MIGRATE_MEM_OBJECT_HOST, &compute_event, &done_event[0]);
            clWaitForEvents(1, (const cl_event *)&done_event[0]);
            time_lzw.stop();

            cout << "Associative Mem count is : " << *assoc_mem << endl;

            if (*failure) {
                printf("FAILED TO INSERT INTO ASSOC MEM!!\n");
                exit(EXIT_FAILURE);
            }

            packet_len = ((*out_packet_length * 12) / 8);
            packet_len = (chunk_idx == -1 && (*out_packet_length % 2 != 0)) ? packet_len + 1 : packet_len;

            unsigned char *data_packet = create_packet(chunk_idx, *out_packet_length, host_output_lzw, packet_len);

            header = packet_len << 1;
            fwrite(&header, sizeof(uint32_t), 1, fptr_write);
            lzw_bytes += 4;

            fwrite(data_packet, sizeof(unsigned char), packet_len, fptr_write);
            lzw_bytes += packet_len;

            free(data_packet);
            /* free(out_packet); */
        } else {
            header = (chunk_idx << 1) | 1;
            fwrite(&header, sizeof(uint32_t), 1, fptr_write);
            dedup_bytes += 4;
        }
    }
    total_time.stop();
    // q.finish();

    // Print Latencies
    std::cout << "Total latency of CDC is: " << time_cdc.latency() << " ns." << std::endl;
    std::cout << "Total latency of LZW is: " << time_lzw.latency() << " ns." << std::endl;
    std::cout << "Total latency of SHA256 is: " << time_sha.latency() << " ns." << std::endl;
    std::cout << "Total latency of DeDup is: " << time_dedup.latency() << " ns." << std::endl;
    std::cout << "Total time taken: " << total_time.latency() << " ns." << std::endl;
    std::cout << "Total Kernel Execution Time using Profiling Info: " << total_time_2 << " ns." << std::endl;
    std::cout << "---------------------------------------------------------------" << std::endl;
    std::cout << "Average latency of CDC per loop iteration is: " << time_cdc.avg_latency() << " ns." << std::endl;
    std::cout << "Average latency of LZW per loop iteration is: " << time_lzw.avg_latency() << " ns." << std::endl;
    std::cout << "Average latency of SHA256 per loop iteration is: " << time_sha.avg_latency() << " ns." << std::endl;
    std::cout << "Average latency of DeDup per loop iteration is: " << time_dedup.avg_latency() << " ns." << std::endl;
    std::cout << "Average latency: " << total_time.avg_latency() << " ns." << std::endl;
}

int main(int argc, char *argv[]) {

    stopwatch ethernet_timer;
    stopwatch compression_timer;
    unsigned char *input[NUM_PACKETS];
    int writer = 0;
    int done = 0;
    int length = -1;
    uint64_t offset = 0;
    int sum = 0;
    ESE532_Server server;

    int blocksize = BLOCKSIZE;
    char *file = strdup("compressed_file.bin");

    // set blocksize if decalred through command line
    handle_input(argc, argv, &blocksize, &file);

    FILE *fptr_write = fopen(file, "wb");
    if (fptr_write == NULL) {
        printf("Error creating file for compressed output!!\n");
        exit(EXIT_FAILURE);
    }

    unsigned char *pipeline_buffer = (unsigned char *)calloc(NUM_PACKETS * blocksize, sizeof(unsigned char));
    CHECK_MALLOC(pipeline_buffer, "Unable to allocate memory for pipeline buffer");

    for (int i = 0; i < (NUM_PACKETS); i++) {
        input[i] = (unsigned char *)calloc((blocksize + HEADER), sizeof(unsigned char));
        CHECK_MALLOC(input, "Unable to allocate memory for input buffer");
    }

    server.setup_server(blocksize);

    writer = 0;

    // ------------------------------------------------------------------------------------
    // Step 1: Initialize the OpenCL environment
    // ------------------------------------------------------------------------------------
    cl_int err;
    std::string binaryFile = argv[1];
    unsigned fileBufSize;
    std::vector<cl::Device> devices = get_xilinx_devices();
    devices.resize(1);
    cl::Device device = devices[0];
    cl::Context context(device, NULL, NULL, NULL, &err);
    char *fileBuf = read_binary_file(binaryFile, fileBufSize);
    cl::Program::Binaries bins{{fileBuf, fileBufSize}};
    cl::Program program(context, devices, bins, NULL, &err);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE, &err);
    cl::Kernel krnl_lzw(program, "lzw", &err);

    // ------------------------------------------------------------------------------------
    // Step 2: Create buffers and initialize test values
    // ------------------------------------------------------------------------------------

    cl::Buffer lzw_input_buffer = cl::Buffer(context, CL_MEM_READ_ONLY, sizeof(unsigned char) * NUM_PACKETS * blocksize, NULL, &err);
    cl::Buffer lzw_output_buffer = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t) * MAX_CHUNK_SIZE, NULL, &err);

    // Writing to host_input_chunk will fill up lzw_input_buffer.
    unsigned char *host_input_chunk = (unsigned char *)q.enqueueMapBuffer(lzw_input_buffer, CL_TRUE, CL_MAP_WRITE, 0,
                                                                          sizeof(unsigned char) * NUM_PACKETS * blocksize);
    // LZW's output will be written into host_output_lzw.
    uint32_t *host_output_lzw =
        (uint32_t *)q.enqueueMapBuffer(lzw_output_buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(uint32_t) * MAX_CHUNK_SIZE);

    cl::Buffer out_packet_length_buf = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t), NULL, &err);
    uint32_t *out_packet_length =
        (uint32_t *)q.enqueueMapBuffer(out_packet_length_buf, CL_TRUE, CL_MAP_READ, 0, sizeof(uint32_t));

    cl::Buffer failure_buf = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint8_t), NULL, &err);
    uint8_t *failure = (uint8_t *)q.enqueueMapBuffer(failure_buf, CL_TRUE, CL_MAP_READ, 0, sizeof(uint8_t));

    cl::Buffer assoc_mem_buf = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(unsigned int), NULL, &err);
    uint32_t *assoc_mem =
        (uint32_t *)q.enqueueMapBuffer(assoc_mem_buf, CL_TRUE, CL_MAP_READ, 0, sizeof(unsigned int));

    // last message
    while (!done) {
        // while (length != 0) {
        // reset ring buffer

        ethernet_timer.start();
        server.get_packet(input[writer]);
        ethernet_timer.stop();

        // get packet
        unsigned char *buffer = input[writer];

        // decode
        done = buffer[1] & DONE_BIT_L;
        length = buffer[0] | (buffer[1] << 8);
        length &= ~DONE_BIT_H;

        offset += length;
        sum += length;

        // Perform the actual computation here. The idea is to maintain a
        // buffer, that will hold multiple packets, so that CDC can chunklength)
        // at appropriate boundaries. Call the compression pipeline function
        // after the buffer is completely filled.
        if (length != 0)
            memcpy(pipeline_buffer + (writer * blocksize), input[writer] + 2, length);
        // memcpy(pipeline_buffer + (writer * 1024), input[writer], length);

        if (writer == (NUM_PACKETS - 1) || (length < blocksize && length > 0) || done == 1) {
            // if (writer == (NUM_PACKETS - 1) || (length < 1024 && length > 0))
            // {
            compression_timer.start();
            // compression_pipeline(pipeline_buffer, sum, fptr_write);
            compression_pipeline(pipeline_buffer, sum, fptr_write, q, krnl_lzw, lzw_input_buffer, lzw_output_buffer,
                                 host_input_chunk, host_output_lzw, out_packet_length_buf, out_packet_length, failure_buf,
                                 failure, assoc_mem_buf, assoc_mem);
            compression_timer.stop();
            writer = 0;
            sum = 0;
        } else
            writer += 1;
    }

    free(pipeline_buffer);

    for (int i = 0; i < (NUM_PACKETS); i++)
        free(input[i]);

    // fclose(fptr);
    fclose(fptr_write);
    q.finish();

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
    cout << "Bytes Contributed by Deduplication: " << dedup_bytes << "B." << endl;
    cout << "Bytes Contributed by LZW: " << lzw_bytes << "B." << endl;

    return 0;
}
