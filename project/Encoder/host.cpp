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

#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

using namespace std;

uint64_t dedup_bytes = 0;
uint64_t lzw_bytes = 0;
uint64_t lzw_size = 0;

stopwatch time_cdc;
stopwatch time_lzw;
stopwatch time_sha;
stopwatch time_dedup;
stopwatch total_time;

void handle_input(int argc, char *argv[], int *blocksize, char **filename, char **kernel_name) {
    int x;
    extern char *optarg;

    while ((x = getopt(argc, argv, ":b:f:k:")) != -1) {
        switch (x) {
        case 'k':
            *kernel_name = optarg;
            printf("Kernel name is set to %s optarg\n", *kernel_name);
            break;
        case 'b':
            *blocksize = atoi(optarg);
            printf("blocksize is set to %d optarg\n", *blocksize);
            break;
        case 'f':
            *filename = optarg;
            printf("filename is %s optarg\n", *filename);
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
                data[data_idx] = ((current_val) & 0xFF);
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
                                 unsigned char *host_input_chunk, uint32_t *host_output_lzw,
                                 cl::Buffer out_packet_length_buf, uint32_t *out_packet_length, cl::Buffer failure_buf,
                                 uint8_t *failure, cl::Buffer assoc_mem_buf, unsigned int *assoc_mem) {

    vector<uint32_t> vect;
    string sha_fingerprint;
    int64_t chunk_idx = 0;
    /* uint32_t out_packet_length = 0; */
    /* uint32_t *out_packet = NULL; */
    uint32_t packet_len = 0;
    uint32_t header = 0;
    /* uint8_t failure = 0; */
    /* unsigned int assoc_mem = 0; */
    vector<pair<pair<int, int>, unsigned char *>> final_data;

    // ------------------------------------------------------------------------------------
    // Step 3: Run the kernel
    // ------------------------------------------------------------------------------------

    std::vector<cl::Event> write_event(1);
    std::vector<cl::Event> compute_event(1);
    std::vector<cl::Event> done_event(1);

    // double total_time_2 = 0;

    total_time.start();
    // RUN CDC
    time_cdc.start();
    cdc(input, length_sum, vect);
    time_cdc.stop();

    cout << "Vect Size: " << vect.size() << "." << endl;

    memcpy(host_input_chunk, input, sizeof(unsigned char) * length_sum);

    for (int i = 0; i < (int)(vect.size() - 1); i++) {

        lzw_size += vect[i + 1] - vect[i];

        // RUN SHA
        time_sha.start();
        sha_fingerprint = sha_256(input, vect[i], vect[i + 1]);
        time_sha.stop();

        // RUN DEDUP
        time_dedup.start();
        chunk_idx = dedup(sha_fingerprint);
        time_dedup.stop();

        // CDC Output	- [ 0, 23, 500, 2000, 4000, 8000, 9000, 12000, 15000, 16000]
        // Chunk Indices - [	-1,  23,   -1,   -1,   -1,   44,	-1,	99,	-1]

        // Create a vector that holds all the data. This will store header with
        // data in the case of a unique chunk and store only the header in case
        // of a duplicate chunk.

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

            // Profiling the kernel.
            // compute_event[0].wait();
            // total_time_2 += compute_event[0].getProfilingInfo<CL_PROFILING_COMMAND_END>() -
            // compute_event[0].getProfilingInfo<CL_PROFILING_COMMAND_START>();

            q.enqueueMigrateMemObjects({lzw_output_buffer, out_packet_length_buf, failure_buf, assoc_mem_buf},
                                       CL_MIGRATE_MEM_OBJECT_HOST, &compute_event, &done_event[0]);
            clWaitForEvents(1, (const cl_event *)&done_event[0]);
            time_lzw.stop();

            //*failure = 0;
            //*assoc_mem = 0;
            //*out_packet_length = vect[i+1] - vect[i];
            // host_output_lzw = (uint32_t *)host_input_chunk;

            cout << "Associative Mem count is : " << *assoc_mem << endl;

            if (*failure) {
                printf("FAILED TO INSERT INTO ASSOC MEM!!\n");
                exit(EXIT_FAILURE);
            }

            packet_len = ((*out_packet_length * 12) / 8);
            packet_len = (chunk_idx == -1 && (*out_packet_length % 2 != 0)) ? packet_len + 1 : packet_len;

            unsigned char *data_packet = create_packet(chunk_idx, *out_packet_length, host_output_lzw, packet_len);

            header = packet_len << 1;
            // fwrite(&header, sizeof(uint32_t), 1, fptr_write);
            final_data.push_back({{header, packet_len}, data_packet});
            lzw_bytes += 4;

            // fwrite(data_packet, sizeof(unsigned char), packet_len, fptr_write);
            lzw_bytes += packet_len;

            // free(data_packet);
            /* free(out_packet); */
        } else {
            header = (chunk_idx << 1) | 1;
            // fwrite(&header, sizeof(uint32_t), 1, fptr_write);
            final_data.push_back({{header, -1}, NULL});
            dedup_bytes += 4;
        }
    }

    // For loop that iterates over the chunks and performs LZW.

    total_time.stop();

    for (auto it : final_data) {
        if (it.second != NULL) {
            fwrite(&it.first.first, sizeof(uint32_t), 1, fptr_write);
            fwrite(it.second, sizeof(unsigned char), it.first.second, fptr_write);
            free(it.second);
        } else {
            fwrite(&it.first.first, sizeof(uint32_t), 1, fptr_write);
        }
    }

    // cout << "Total Kernel Execution Time using Profiling Info: " << total_time_2 << " ms." << endl;
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
    char *kernel_name = strdup("lzw.xclbin");

    // set blocksize if decalred through command line
    handle_input(argc, argv, &blocksize, &file, &kernel_name);

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
    std::string binaryFile = kernel_name;
    unsigned fileBufSize;
    std::vector<cl::Device> devices = get_xilinx_devices();
    devices.resize(1);
    cl::Device device = devices[0];
    cl::Context context(device, NULL, NULL, NULL, &err);
    char *fileBuf = read_binary_file(binaryFile, fileBufSize);
    cl::Program::Binaries bins{{fileBuf, fileBufSize}};
    cl::Program program(context, devices, bins, NULL, &err);
    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);
    cl::Kernel krnl_lzw(program, "lzw", &err);

    // ------------------------------------------------------------------------------------
    // Step 2: Create buffers and initialize test values
    // ------------------------------------------------------------------------------------

    cl::Buffer lzw_input_buffer =
        cl::Buffer(context, CL_MEM_READ_ONLY, sizeof(unsigned char) * NUM_PACKETS * blocksize, NULL, &err);
    cl::Buffer lzw_output_buffer =
        cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t) * MAX_CHUNK_SIZE, NULL, &err);

    // Writing to host_input_chunk will fill up lzw_input_buffer.
    unsigned char *host_input_chunk = (unsigned char *)q.enqueueMapBuffer(
        lzw_input_buffer, CL_TRUE, CL_MAP_WRITE, 0, sizeof(unsigned char) * NUM_PACKETS * blocksize);
    // LZW's output will be written into host_output_lzw.
    uint32_t *host_output_lzw =
        (uint32_t *)q.enqueueMapBuffer(lzw_output_buffer, CL_TRUE, CL_MAP_READ, 0, sizeof(uint32_t) * MAX_CHUNK_SIZE);

    cl::Buffer out_packet_length_buf = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint32_t), NULL, &err);
    uint32_t *out_packet_length =
        (uint32_t *)q.enqueueMapBuffer(out_packet_length_buf, CL_TRUE, CL_MAP_READ, 0, sizeof(uint32_t));

    cl::Buffer failure_buf = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(uint8_t), NULL, &err);
    uint8_t *failure = (uint8_t *)q.enqueueMapBuffer(failure_buf, CL_TRUE, CL_MAP_READ, 0, sizeof(uint8_t));

    cl::Buffer assoc_mem_buf = cl::Buffer(context, CL_MEM_WRITE_ONLY, sizeof(unsigned int), NULL, &err);
    uint32_t *assoc_mem = (uint32_t *)q.enqueueMapBuffer(assoc_mem_buf, CL_TRUE, CL_MAP_READ, 0, sizeof(unsigned int));

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
                                 host_input_chunk, host_output_lzw, out_packet_length_buf, out_packet_length,
                                 failure_buf, failure, assoc_mem_buf, assoc_mem);
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
    q.enqueueUnmapMemObject(lzw_input_buffer, host_input_chunk);
    q.enqueueUnmapMemObject(lzw_output_buffer, host_output_lzw);
    q.enqueueUnmapMemObject(out_packet_length_buf, out_packet_length);
    q.enqueueUnmapMemObject(failure_buf, failure);
    q.enqueueUnmapMemObject(assoc_mem_buf, assoc_mem);
    q.finish();

    // Print Latencies
    cout << "--------------- Total Latencies ---------------" << endl;
    cout << "Total latency of CDC is: " << time_cdc.latency() << " ms." << endl;
    cout << "Total latency of LZW is: " << time_lzw.latency() << " ms." << endl;
    cout << "Total latency of SHA256 is: " << time_sha.latency() << " ms." << endl;
    cout << "Total latency of DeDup is: " << time_dedup.latency() << " ms." << endl;
    cout << "Total time taken: " << total_time.latency() << " ns." << endl;
    cout << "---------------- Average Latencies ------------" << endl;
    cout << "Average latency of CDC per loop iteration is: " << time_cdc.avg_latency() << " ms." << endl;
    cout << "Average latency of LZW per loop iteration is: " << time_lzw.avg_latency() << " ms." << endl;
    cout << "Average latency of SHA256 per loop iteration is: " << time_sha.avg_latency() << " ms." << endl;
    cout << "Average latency of DeDup per loop iteration is: " << time_dedup.avg_latency() << " ms." << endl;
    cout << "Average latency: " << total_time.avg_latency() << " ms." << std::endl;

    std::cout << "\n\n";

    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    float compression_latency = compression_timer.latency() / 1000.0;
    float compression_latency_total_time = total_time.latency() / 1000.0;

    float cdc_latency_total_time = time_cdc.latency() / 1000.0;
    float sha_latency_total_time = time_sha.latency() / 1000.0;
    float lzw_latency_total_time = time_lzw.latency() / 1000.0;
    float dedup_latency_total_time = time_dedup.latency() / 1000.0;

    float compression_throughput = (offset * 8 / 1000000.0) / compression_latency;              // Mb/s
    float compression_throughput_2 = (offset * 8 / 1000000.0) / compression_latency_total_time; // Mb/s

    float cdc_throughput = (offset * 8 / 1000000.0) / cdc_latency_total_time;     // Mb/s
    float sha_throughput = (offset * 8 / 1000000.0) / sha_latency_total_time;     // Mb/s
    float lzw_throughput = (lzw_size * 8 / 1000000.0) / lzw_latency_total_time;   // Mb/s
    float dedup_throughput = (offset * 8 / 1000000.0) / dedup_latency_total_time; // Mb/s

    float ethernet_throughput = (offset * 8 / 1000000.0) / ethernet_latency; // Mb/s
    // std::cout << "Throughput of the Encoder: " << throughput << " Mb/s."
    //     	<< " (Ethernet Latency: " << ethernet_latency << "s)." <<
    //     	std::endl;
    cout << "Ethernet Latency: " << ethernet_latency << "s." << endl;
    cout << "Bytes Received: " << offset << "B." << endl;
    cout << "Latency for Compression: " << compression_latency << "s." << endl;

    cout << "Latency for CDC: " << cdc_latency_total_time << "s." << endl;
    cout << "Latency for SHA: " << sha_latency_total_time << "s." << endl;
    cout << "Latency for LZW: " << lzw_latency_total_time << "s." << endl;
    cout << "Latency for DEDUP: " << dedup_latency_total_time << "s." << endl;

    cout << "Latency for Compression (without fwrite): " << compression_latency_total_time << "s." << endl;
    cout << "Ethernet Throughput: " << ethernet_throughput << "Mb/s." << endl;

    cout << "CDC Throughput: " << cdc_throughput << "Mb/s." << endl;
    cout << "SHA Throughput: " << sha_throughput << "Mb/s." << endl;
    cout << "LZW Throughput: " << lzw_throughput << "Mb/s." << endl;
    cout << "DEDUP Throughput: " << dedup_throughput << "Mb/s." << endl;

    cout << "Application Throughput: " << compression_throughput << "Mb/s." << endl;
    cout << "Application Throughput (without fwrite): " << compression_throughput_2 << "Mb/s." << endl;
    cout << "Bytes Contributed by Deduplication: " << dedup_bytes << "B." << endl;
    cout << "Bytes Contributed by LZW: " << lzw_bytes << "B." << endl;

    return 0;
}
