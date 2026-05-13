
#include <cstdint>
#include <iostream>
#include <cstddef>

struct Packet{
    uint8_t id;
    double temperature;
    uint16_t crc;
};

int main() {
    
    std::cout << sizeof(Packet) << std::endl;
    std::cout << offsetof(Packet, id) << std::endl;
    std::cout << offsetof(Packet, temperature) << std::endl;
    std::cout << offsetof(Packet, crc) << std::endl;
    
    return 0;
}