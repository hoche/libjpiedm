// examples/header_checksum_generator.cpp
// Utility to build JPI EDM-style header lines with XOR checksum

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {
unsigned int calculateChecksum(const std::string &payload)
{
    unsigned int cs = 0;
    for (unsigned char ch : payload) {
        cs ^= ch;
    }
    return cs & 0xFF;
}
} // namespace

int main(int argc, char **argv)
{
    // Accept payload either from command-line argument or stdin
    if (argc > 1) {
        std::string payload = argv[1];
        const unsigned int checksum = calculateChecksum(payload);

        std::ostringstream line;
        line << '$' << payload << '*' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << checksum;

        std::cout << line.str() << "\r\n";
        return 0;
    }

    std::cerr << "Enter header payloads (without leading '$' or checksum)." << std::endl;
    std::cerr << "Press Ctrl+D (Unix) or Ctrl+Z (Windows) to finish." << std::endl;

    std::string payload;
    while (std::getline(std::cin, payload)) {
        const unsigned int checksum = calculateChecksum(payload);
        std::cout << '$' << payload << '*' << std::uppercase << std::setfill('0') << std::setw(2) << std::hex
                  << checksum << "\r\n";
    }

    return 0;
}
