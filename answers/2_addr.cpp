

#include <cstdint>
#include <sys/mman.h>
#include <iostream>
using namespace std;

int main() {
    // Map a page at a chosen base address
    void* base = mmap(nullptr, 4096, PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED) { perror("mmap"); return 1; }
    uintptr_t addr = (uintptr_t)base + 0x14;

    // cout << reinterpret_cast<int*>(addr) << endl;
    // &auto static_cast<auto*>(addr) = 0x20;
    // *reinterpret_cast<volatile uint32_t*>(0x40020014) = 0x20;

    *reinterpret_cast<volatile uint32_t*>(addr) = 0x20;
    cout << *reinterpret_cast<volatile uint32_t*>(addr) << endl;

    // uint32_t v = *reinterpret_cast<volatile uint32_t*>(addr);
    // cout << v << endl;

    // int i = 5;
    // int* p = &i;
    // cout << *p << endl;
    
    return 0;
}