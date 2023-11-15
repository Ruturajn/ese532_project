#define CL_HPP_CL_1_2_DEFAULT_BUILD
#define CL_HPP_TARGET_OPENCL_VERSION 120
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_PROGRAM_CONSTRUCTION_FROM_ARRAY_COMPATIBILITY 1
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#include "EventTimer.h"
#include <CL/cl2.hpp>
#include <cstdint>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <unistd.h>
#include <vector>
#include "Pipeline.h"
#include "Utilities.h"
#include <chrono>
#include "../Server/stopwatch.h"

int main (int argc, char *argv[])
{
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

    cl::CommandQueue q(context, device, CL_QUEUE_PROFILING_ENABLE | CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, &err);

    cl::Kernel krnl_filter(program, "Filter_HW", &err);
    size_t elements_per_iteration = SCALED_FRAME_WIDTH * SCALED_FRAME_HEIGHT;
    size_t bytes_per_iteration = elements_per_iteration * sizeof(unsigned char);
    cl::Buffer in_buf[FRAMES];
    cl::Buffer out_buf[FRAMES];

    for(int i = 0; i < FRAMES; i++)
    {
        in_buf[i] = cl::Buffer(context, CL_MEM_READ_ONLY, bytes_per_iteration, NULL, &err);
        out_buf[i] = cl::Buffer(context, CL_MEM_READ_ONLY, bytes_per_iteration, NULL, &err);
    }

    unsigned char *Input[FRAMES];
    unsigned char *Output[FRAMES];
    for(int i = 0; i < FRAMES; i++)
    {
        Input[i] = (unsigned char*)q.enqueueMapBuffer(in_buf[i], CL_TRUE, CL_MAP_WRITE, 0, bytes_per_iteration);
        Output[i] = (unsigned char*)q.enqueueMapBuffer(out_buf[i], CL_TRUE, CL_MAP_WRITE, 0, bytes_per_iteration);
    }

    unsigned char *Input_data = (unsigned char *)malloc(FRAMES * FRAME_SIZE);
    unsigned char *output_hw = (unsigned char *)malloc(FRAMES * FRAME_SIZE);
    unsigned char *differentiate_output = (unsigned char *)malloc(FRAMES * FRAME_SIZE);
  
    Load_data (Input_data);
    
    stopwatch time;
    int Size;
    time.start();

    for(int Frame = 0; Frame < FRAMES ;Frame++){

        Scale_SW(Input_data + Frame * FRAME_SIZE, Input[Frame]);

        std::vector<cl::Event> exec_events, write_events;
        std::vector<cl::Event> read_events;
        cl::Event write_ev; 
        cl::Event exec_ev;
        cl::Event read_ev;
        krnl_filter.setArg(0,in_buf[Frame]);
        krnl_filter.setArg(1,out_buf[Frame]); 

        if(Frame == 0)
            q.enqueueMigrateMemObjects({in_buf[Frame]}, 0 ,NULL, &write_ev);
        else
            q.enqueueMigrateMemObjects({in_buf[Frame]}, 0, &read_events, &write_ev);

        write_events.push_back(write_ev);
        q.enqueueTask(krnl_filter, &write_events, &exec_ev);
        exec_events.push_back(exec_ev);
        q.enqueueMigrateMemObjects({out_buf[Frame]}, CL_MIGRATE_MEM_OBJECT_HOST, &exec_events, &read_ev);
        read_events.push_back(read_ev);

        Differentiate_SW(Output[Frame], differentiate_output);

        Size = Compress_SW(differentiate_output, output_hw);
    }
    
    time.stop();
    std::cout << "Total time taken is: " << time.latency() << " ns." << std::endl;
    Store_data("Output.bin", output_hw, Size);
    q.finish();

// Check_data(output_hw, Size);
    free(Input_data);
    free(differ_output);
    free(output_hw);
}