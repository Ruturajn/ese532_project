#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace std;

int main() {
    static std::ifstream Input;
    Input.open("compressed_file.bin", std::ios::binary);
    if (!Input.good()) {
        std::cerr << "Could not open input file.\n";
        return EXIT_FAILURE;
    }

    unsigned char Header;
    Header = Input.get();
    printf("%x\n", Header);
    Header = Input.get();
    printf("%x\n", Header);
    Header = Input.get();
    printf("%x\n", Header);
    Header = Input.get();
    printf("%x\n", Header);
    Header = Input.get();
    printf("%x\n", Header);
    Header = Input.get();
    printf("%x\n", Header);
    Header = Input.get();
    printf("%x\n", Header);

    /*
        unsigned char Header;
        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);

        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);

        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);

        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);

        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);

        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);
        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);

        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);
        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);

        Input.read((char *) &Header, sizeof(unsigned char));
        printf("%x\n", Header);
        */

    /*
    uint32_t Header;
    Input.read((char *) &Header, sizeof(int32_t));
    printf("%x", Header);
    */

    putchar('\n');

    return 0;
}