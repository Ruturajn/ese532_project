#include "common.h"

void cdc(unsigned char *buff, unsigned int buff_size,
         unsigned int start_idx, unsigned int end_idx,
         std::vector<uint64_t>& vect)
{
    // put your cdc implementation here
    uint64_t hash = hash_func(buff, start_idx);
    for (unsigned int i = start_idx; i < end_idx; i++){
        if (hash % MODULUS == TARGET)
                vect.push_back(i);
        hash = (hash * PRIME) -
                (buff[i] * pow(PRIME, WIN_SIZE + 1)) +
                (buff[i + WIN_SIZE] * PRIME);
    }
}

// void test_cdc( const char* file )
// {
//     FILE* fp = fopen(file,"r" );
//     if(fp == NULL ){
//             perror("fopen error");
//             return;
//     }
// 
//     fseek(fp, 0, SEEK_END); // seek to end of file
//     int file_size = ftell(fp); // get current file pointer
//     fseek(fp, 0, SEEK_SET); // seek back to beginning of file
// 
//     unsigned char* buff = (unsigned char *)malloc((sizeof(unsigned char) * file_size ));
//     if(buff == NULL)
//     {
//             perror("not enough space");
//             fclose(fp);
//             return;
//     }
// 
//     int bytes_read = fread(&buff[0],sizeof(unsigned char),file_size,fp);
//     if(bytes_read != file_size)
//     {
//             perror("not enough bytes read");
//             fclose(fp);
//             return;
//     }
// 
//     // parallelize cdc over 4 threads here
//     stopwatch time_cdc;
// 
//     time_cdc.start();
// 
// 
//     std::vector<std::thread> ths;
//     std::vector<uint64_t> vect1;
//     std::vector<uint64_t> vect2;
//     std::vector<uint64_t> vect3;
//     std::vector<uint64_t> vect4;
// 
//     int offset = file_size / 4;
//     ths.push_back(std::thread(&cdc, std::ref(buff), file_size,
//                     WIN_SIZE, offset, std::ref(vect1)));
//     ths.push_back(std::thread(&cdc, std::ref(buff), file_size,
//                     offset, 2*offset, std::ref(vect2)));
//     ths.push_back(std::thread(&cdc, std::ref(buff), file_size,
//                     2*offset, 3*offset, std::ref(vect3)));
//     ths.push_back(std::thread(&cdc, std::ref(buff), file_size,
//                     3*offset, file_size - WIN_SIZE, std::ref(vect4)));
// 
// 	pin_thread_to_cpu(ths[0], 0);
// 	pin_thread_to_cpu(ths[1], 1);
// 	pin_thread_to_cpu(ths[2], 2);
// 	pin_thread_to_cpu(ths[3], 3);
// 
//     for (auto &th: ths) {
//             th.join();
//     }
//     time_cdc.stop();
// 
//     for (auto &i: vect1) {
//             std::cout << i << std::endl;
//     }
// 
//     for (auto &i: vect2) {
//             std::cout << i << std::endl;
//     }
// 
//     for (auto &i: vect3) {
//             std::cout << i << std::endl;
//     }
// 
//     for (auto &i: vect4) {
//             std::cout << i << std::endl;
//     }
//     std::cerr << "Total latency of CDC is: " << time_cdc.latency() << " ns." << std::endl;
// 
//     free(buff);
//     return;
// }
// 
// int main()
// {
//         test_cdc("../data/prince.txt");
//         return 0;
// }