#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

std::string ByteArrayToHexString(unsigned char *bytes, int size) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (int i = 0; i < size; i++) {
        ss << std::setw(2) << static_cast<int>(bytes[i]);
    }
    return ss.str();
}

int main() {
    // Sample byte array
    unsigned char byteArray[5] = {0x12, 0x34, 0xAB, 0xCD, 0x02};

    std::string hexString = ByteArrayToHexString(byteArray, 5);
    std::cout << "Hex string: " << hexString << std::endl;

    return 0;
}
