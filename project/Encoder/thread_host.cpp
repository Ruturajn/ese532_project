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
#include <unordered_map>
#include <vector>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <atomic>

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

stopwatch time_cdc;
stopwatch time_lzw;
stopwatch time_sha;
stopwatch time_dedup;
stopwatch total_time;

std::mutex m;
std::condition_variable cv;
bool ready = false;
bool processed = false;
unsigned int chunk_size = CHUNK_SIZE;
unsigned char *collected_data = NULL;
std::atomic<bool> done_flag(false);
FILE *fptr_write = NULL;

typedef struct __attribute__((packed)) RawData {
    int length_sum;                 /// The total length of the input buffer.
    unsigned char *pipeline_buffer; /// Input data that needs to be processed.
    FILE *fptr_write;               /// File pointer to write the output.
    unsigned char *host_input;
    uint32_t *output_codes;
    uint32_t *chunk_indices;
    uint32_t *output_code_lengths;
} RawData;

typedef struct CLDevice {
    cl::Kernel kernel;
    cl::CommandQueue queue;
} CLDevice;

void handle_input(int argc, char *argv[], int *blocksize, char **filename,
                  char **kernel_name, unsigned int *chunk_size) {
    int x;
    extern char *optarg;

    while ((x = getopt(argc, argv, ":b:f:k:c:")) != -1) {
        switch (x) {
        case 'k':
            *kernel_name = optarg;
            printf("Kernel name is set to %s optarg\n", *kernel_name);
            break;
        case 'b':
            *blocksize = atoi(optarg);
            printf("blocksize is set to %d optarg\n", *blocksize);
            break;
        case 'c':
            *chunk_size = atoi(optarg);
            printf("Chunk size is set to %d optarg\n", *chunk_size);
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

static unsigned char *create_packet(int32_t chunk_idx,
                                    uint32_t out_packet_length,
                                    uint32_t *out_packet, uint32_t packet_len) {
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

RawData *r_data;
unsigned char *host_input;
uint32_t *chunk_indices;
uint32_t *stat_data;
CLDevice dev;
cl::Buffer lzw_input_buffer;
cl::Buffer chunk_indices_buffer;
cl::Buffer stat_data_buffer;
int64_t* dedup_out_data;
cl::Buffer dedup_out_buffer;
unsigned char *bit_packed_data;
cl::Buffer bit_packed_data_buffer;
int thread_length_sum;

char *file = NULL;

static void compression_pipeline() {

    fptr_write = fopen(file, "wb");
    if (fptr_write == NULL) {
        printf("Error creating file for compressed output!!\n");
        exit(EXIT_FAILURE);
    }


    while (true) {

        std::unique_lock<mutex> lk(m);
        cv.wait(lk, []{ return ready; });

        if (done_flag.load())
            break;

        cout << "In worker thread lock acquired!!" << endl;

        vector<uint32_t> vect;
        string sha_fingerprint;
        int64_t chunk_idx = 0;
        // uint32_t packet_len = 0;
        // uint32_t header = 0;
        vector<int64_t> dedup_out;
        vector<pair<pair<int, int>, unsigned char *>> final_data;

        // ------------------------------------------------------------------------------------
        // Step 3: Run the kernel
        // ------------------------------------------------------------------------------------

        std::vector<cl::Event> write_event(1);
        std::vector<cl::Event> compute_event(1);
        std::vector<cl::Event> done_event(1);

        // double total_time_2 = 0;

        memcpy(host_input, collected_data,
               sizeof(unsigned char) * thread_length_sum);

        total_time.start();

        // RUN CDC
        time_cdc.start();
        fast_cdc(collected_data, thread_length_sum, chunk_size, vect);
        // cdc(r_data->pipeline_buffer, r_data->length_sum, vect);
        time_cdc.stop();

        chunk_indices[0] = vect.size();

        std::copy(vect.begin(), vect.end(), chunk_indices + 1);

        for (int i = 0; i < (int)(vect.size() - 1); i++) {
            // RUN SHA
            time_sha.start();
            sha_fingerprint =
            sha_256(collected_data, vect[i], vect[i + 1]);
            time_sha.stop();

            // RUN DEDUP
            time_dedup.start();
            chunk_idx = dedup(sha_fingerprint);
            dedup_out.push_back(chunk_idx);
            time_dedup.stop();
        }

        // RUN LZW
        time_lzw.start();

        std::copy(dedup_out.begin(), dedup_out.end(), dedup_out_data);

        dev.kernel.setArg(0, lzw_input_buffer);
        dev.kernel.setArg(1, bit_packed_data_buffer);
        /* dev.kernel.setArg(2, lzw_output_buffer); */
        dev.kernel.setArg(2, chunk_indices_buffer);
        dev.kernel.setArg(3, stat_data_buffer);
        dev.kernel.setArg(4, dedup_out_buffer);

        dev.queue.enqueueMigrateMemObjects({lzw_input_buffer, chunk_indices_buffer, dedup_out_buffer},
                                           0 /* 0 means from host*/, NULL,
                                           &write_event[0]);

        dev.queue.enqueueTask(dev.kernel, &write_event, &compute_event[0]);

        // Profiling the kernel.
        /* compute_event[0].wait(); */
        /* total_time_2 +=
         * compute_event[0].getProfilingInfo<CL_PROFILING_COMMAND_END>() - */
        /* compute_event[0].getProfilingInfo<CL_PROFILING_COMMAND_START>(); */

        dev.queue.enqueueMigrateMemObjects(
            {bit_packed_data_buffer, stat_data_buffer},
            CL_MIGRATE_MEM_OBJECT_HOST, &compute_event, &done_event[0]);
        clWaitForEvents(1, (const cl_event *)&done_event[0]);
        time_lzw.stop();

        if (stat_data[0] & 0x1) {
            printf("FAILED TO INSERT INTO ASSOC MEM!!\n");
            exit(EXIT_FAILURE);
        }

        // uint32_t *output_codes_ptr = output_codes;

        // for (int i = 1; i < (int)chunk_indices[0]; i++) {
        //     if (dedup_out[i - 1] == -1) {
        //         packet_len = ((output_code_lengths[i] * 12) / 8);
        //         packet_len =
        //             (output_code_lengths[i] % 2 != 0) ? packet_len + 1 : packet_len;

        //         unsigned char *data_packet =
        //             create_packet(chunk_idx, output_code_lengths[i],
        //                           output_codes_ptr, packet_len);

        //         header = packet_len << 1;
        //         final_data.push_back({{header, packet_len}, data_packet});
        //         lzw_bytes += 4;

        //         lzw_bytes += packet_len;

        //     } else {
        //         header = (dedup_out[i - 1] << 1) | 1;
        //         final_data.push_back({{header, -1}, NULL});
        //         dedup_bytes += 4;
        //     }

        //     output_codes_ptr += output_code_lengths[i];
        // }

        total_time.stop();

        cout << "Bit packed data: " << bit_packed_data[0] << endl;
        cout << "Offset : " << (stat_data[0] << 1) << endl;

        fwrite(bit_packed_data, sizeof(unsigned char) * (stat_data[0] >> 1), 1, fptr_write);

        // for (auto it : final_data) {
        //     if (it.second != NULL) {
        //         fwrite(&it.first.first, sizeof(uint32_t), 1, r_data->fptr_write);
        //         fwrite(it.second, sizeof(unsigned char), it.first.second,
        //                r_data->fptr_write);
        //         free(it.second);
        //     } else {
        //         fwrite(&it.first.first, sizeof(uint32_t), 1, r_data->fptr_write);
        //     }
        // }

        /* cout << "Total Kernel Execution Time using Profiling Info: " << total_time_2 */
        /*      << " ms." << endl; */


        processed = true;
        lk.unlock();
        cv.notify_one();
    }

    fclose(fptr_write);
}

int main(int argc, char *argv[]) {

    stopwatch ethernet_timer;
    stopwatch compression_timer;
    unsigned char *input[NUM_PACKETS];
    int writer = 0;
    int done = 0;
    int length = -1;
    uint64_t offset = 0;
    ESE532_Server server;

    int blocksize = BLOCKSIZE;
    file = strdup("compressed_file.bin");
    char *kernel_name = strdup("lzw.xclbin");

    // set blocksize if decalred through command line
    handle_input(argc, argv, &blocksize, &file, &kernel_name, &chunk_size);

    r_data = (RawData *)calloc(1, sizeof(RawData));
    CHECK_MALLOC(r_data, "Unable to allocate memory for raw data");

    r_data->pipeline_buffer =
        (unsigned char *)calloc(NUM_PACKETS * blocksize, sizeof(unsigned char));
    CHECK_MALLOC(r_data->pipeline_buffer,
                 "Unable to allocate memory for pipeline buffer");

    collected_data =
        (unsigned char *)calloc(NUM_PACKETS * blocksize, sizeof(unsigned char));
    CHECK_MALLOC(collected_data,
                 "Unable to allocate memory for collected data");

    for (int i = 0; i < (NUM_PACKETS); i++) {
        input[i] = (unsigned char *)calloc((blocksize + HEADER),
                                           sizeof(unsigned char));
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
    dev.queue = cl::CommandQueue(context, device,
                                 CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
                                 &err);
    dev.kernel = cl::Kernel(program, "lzw", &err);

    // ------------------------------------------------------------------------------------
    // Step 2: Create buffers and initialize test values
    // ------------------------------------------------------------------------------------

    lzw_input_buffer =
        cl::Buffer(context, CL_MEM_READ_ONLY,
                   sizeof(unsigned char) * NUM_PACKETS * blocksize, NULL, &err);
    host_input = (unsigned char *)dev.queue.enqueueMapBuffer(
        lzw_input_buffer, CL_TRUE, CL_MAP_WRITE, 0,
        sizeof(unsigned char) * NUM_PACKETS * blocksize);

    /* cl::Buffer lzw_output_buffer = */
    /*     cl::Buffer(context, CL_MEM_WRITE_ONLY, */
    /*                sizeof(uint32_t) * MAX_OUTPUT_BUF_SIZE, NULL, &err); */
    /* uint32_t *output_codes = (uint32_t *)dev.queue.enqueueMapBuffer( */
    /*     lzw_output_buffer, CL_TRUE, CL_MAP_READ, 0, */
    /*     sizeof(uint32_t) * MAX_OUTPUT_BUF_SIZE); */

    chunk_indices_buffer =
        cl::Buffer(context, CL_MEM_READ_ONLY, sizeof(uint32_t) * MAX_LZW_CHUNKS,
                   NULL, &err);
    chunk_indices = (uint32_t *)dev.queue.enqueueMapBuffer(
        chunk_indices_buffer, CL_TRUE, CL_MAP_WRITE, 0,
        sizeof(uint32_t) * MAX_LZW_CHUNKS);

    stat_data_buffer =
        cl::Buffer(context, CL_MEM_WRITE_ONLY,
                   sizeof(uint32_t) * 4, NULL, &err);
    stat_data = (uint32_t *)dev.queue.enqueueMapBuffer(
        stat_data_buffer, CL_TRUE, CL_MAP_READ, 0,
        sizeof(uint32_t) * 4);

    dedup_out_buffer =
        cl::Buffer(context, CL_MEM_READ_ONLY, sizeof(int64_t) * MAX_LZW_CHUNKS,
                   NULL, &err);
    dedup_out_data = (int64_t *)dev.queue.enqueueMapBuffer(
        dedup_out_buffer, CL_TRUE, CL_MAP_WRITE, 0,
        sizeof(int64_t) * MAX_LZW_CHUNKS);

    bit_packed_data_buffer =
        cl::Buffer(context, CL_MEM_WRITE_ONLY,
                   sizeof(unsigned char) * MAX_OUTPUT_BUF_SIZE * 4, NULL, &err);
    bit_packed_data = (unsigned char *)dev.queue.enqueueMapBuffer(
        bit_packed_data_buffer, CL_TRUE, CL_MAP_READ, 0,
        sizeof(unsigned char) * MAX_OUTPUT_BUF_SIZE * 4);

    std::thread worker(&compression_pipeline);

    pin_thread_to_cpu(worker, 2);

    bool is_first_iteration = true;

    // Loop until last message
    while (!done) {

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
        r_data->length_sum += length;

        // Perform the actual computation here. The idea is to maintain a
        // buffer, that will hold multiple packets, so that CDC can chunklength)
        // at appropriate boundaries. Call the compression pipeline function
        // after the buffer is completely filled.
        if (length != 0)
            memcpy(r_data->pipeline_buffer + (writer * blocksize),
                   input[writer] + 2, length);

        if (writer == (NUM_PACKETS - 1) || (length < blocksize && length > 0) ||
            done == 1) {
            if (is_first_iteration) {
                // Send data to the worker.
                {
                    cout << "Sending data to the worker!!" << endl;
                    std::lock_guard<mutex> lk(m);
                    ready = true;
                    thread_length_sum = r_data->length_sum;
                    memcpy(collected_data, r_data->pipeline_buffer, r_data->length_sum);
                }
                cv.notify_one();
                is_first_iteration = false;
            } else {
                // Wait for the workder.
                {
                    cout << "Waiting for the worker!!" << endl;
                    std::unique_lock<mutex> lk(m);
                    cv.wait(lk, []{ return processed; });
                }

                // Send data to the worker.
                {
                    cout << "Sending data to the worker!!" << endl;
                    std::lock_guard<mutex> lk(m);
                    ready = true;
                    thread_length_sum = r_data->length_sum;
                    memcpy(collected_data, r_data->pipeline_buffer, r_data->length_sum);
                }
                cv.notify_one();
            }

            // compression_timer.start();
            // compression_pipeline(
            //     r_data, host_input, output_codes, chunk_indices,
            //     output_code_lengths, dev, lzw_input_buffer, lzw_output_buffer,
            //     chunk_indices_buffer, out_packet_lengths_buffer,
            //     dedup_out_data, dedup_out_buffer,
            //     bit_packed_data, bit_packed_data_buffer, chunk_size);
            // compression_timer.stop();
            writer = 0;
            r_data->length_sum = 0;
        } else
            writer += 1;
    }

    // Wait for the workder.
    {
        cout << "Waiting for the worker!!" << endl;
        done_flag = true;
        std::unique_lock<mutex> lk(m);
        cv.wait(lk, []{ return processed; });
    }

    worker.join();

    for (int i = 0; i < (NUM_PACKETS); i++)
        free(input[i]);

    dev.queue.enqueueUnmapMemObject(lzw_input_buffer, host_input);
    /* dev.queue.enqueueUnmapMemObject(lzw_output_buffer, output_codes); */
    dev.queue.enqueueUnmapMemObject(chunk_indices_buffer, chunk_indices);
    dev.queue.enqueueUnmapMemObject(stat_data_buffer,
                                    stat_data);
    dev.queue.finish();

    free(r_data->pipeline_buffer);
    free(r_data);
    free(collected_data);


    // Print Latencies
    cout << "--------------- Total Latencies ---------------" << endl;
    cout << "Total latency of CDC is: " << time_cdc.latency() << " ms." << endl;
    cout << "Total latency of LZW is: " << time_lzw.latency() << " ms." << endl;
    cout << "Total latency of SHA256 is: " << time_sha.latency() << " ms."
         << endl;
    cout << "Total latency of DeDup is: " << time_dedup.latency() << " ms."
         << endl;
    cout << "Total time taken: " << total_time.latency() << " ms." << endl;
    cout << "---------------- Average Latencies ------------" << endl;
    cout << "Average latency of CDC per loop iteration is: "
         << time_cdc.avg_latency() << " ms." << endl;
    cout << "Average latency of LZW per loop iteration is: "
         << time_lzw.avg_latency() << " ms." << endl;
    cout << "Average latency of SHA256 per loop iteration is: "
         << time_sha.avg_latency() << " ms." << endl;
    cout << "Average latency of DeDup per loop iteration is: "
         << time_dedup.avg_latency() << " ms." << endl;
    cout << "Average latency: " << total_time.avg_latency() << " ms."
         << std::endl;

    std::cout << "\n\n";

    std::cout << "--------------- Key Throughputs ---------------" << std::endl;
    float ethernet_latency = ethernet_timer.latency() / 1000.0;
    float compression_latency = compression_timer.latency() / 1000.0;
    float compression_latency_total_time = total_time.latency() / 1000.0;

    float cdc_latency_total_time = time_cdc.latency() / 1000.0;
    float sha_latency_total_time = time_sha.latency() / 1000.0;
    float lzw_latency_total_time = time_lzw.latency() / 1000.0;
    float dedup_latency_total_time = time_dedup.latency() / 1000.0;

    float compression_throughput =
        (offset * 8 / 1000000.0) / compression_latency; // Mb/s
    float compression_throughput_2 =
        (offset * 8 / 1000000.0) / compression_latency_total_time; // Mb/s

    float cdc_throughput =
        (offset * 8 / 1000000.0) / cdc_latency_total_time; // Mb/s
    float sha_throughput =
        (offset * 8 / 1000000.0) / sha_latency_total_time; // Mb/s
    float lzw_throughput =
        (offset * 8 / 1000000.0) / lzw_latency_total_time; // Mb/s
    float dedup_throughput =
        (offset * 8 / 1000000.0) / dedup_latency_total_time; // Mb/s

    float ethernet_throughput =
        (offset * 8 / 1000000.0) / ethernet_latency; // Mb/s

    cout << "Ethernet Latency: " << ethernet_latency << "s." << endl;
    cout << "Bytes Received: " << offset << "B." << endl;
    cout << "Latency for Compression: " << compression_latency << "s." << endl;
    cout << "Latency for Compression (without fwrite): "
         << compression_latency_total_time << "s." << endl;

    std::cout << "\n";

    cout << "Latency for CDC: " << cdc_latency_total_time << "s." << endl;
    cout << "Latency for SHA: " << sha_latency_total_time << "s." << endl;
    cout << "Latency for LZW: " << lzw_latency_total_time << "s." << endl;
    cout << "Latency for DEDUP: " << dedup_latency_total_time << "s." << endl;

    std::cout << "\n";

    cout << "CDC Throughput: " << cdc_throughput << "Mb/s." << endl;
    cout << "SHA Throughput: " << sha_throughput << "Mb/s." << endl;
    cout << "LZW Throughput: " << lzw_throughput << "Mb/s." << endl;
    cout << "DEDUP Throughput: " << dedup_throughput << "Mb/s." << endl;

    std::cout << "\n";

    cout << "Ethernet Throughput: " << ethernet_throughput << "Mb/s." << endl;
    cout << "Application Throughput: " << compression_throughput << "Mb/s."
         << endl;
    cout << "Application Throughput (without fwrite): "
         << compression_throughput_2 << "Mb/s." << endl;
    cout << "Bytes Contributed by Deduplication: " << dedup_bytes << "B."
         << endl;
    cout << "Bytes Contributed by LZW: " << lzw_bytes << "B." << endl;

    std::cout << "\n";

    return 0;
}
