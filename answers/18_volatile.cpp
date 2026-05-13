
#include <cstdint>
#include <iostream>

void read_volatile(){
    volatile uint32_t reg1 = 32;
    uint32_t reg2 = 64;

    uint32_t val1, val2;
    for (int i=0;i<10;i++){ val1 = reg1; }
    for (int i=0;i<10;i++){ val2 = reg2; }
}

int main() {
    read_volatile();
    return 0;
}