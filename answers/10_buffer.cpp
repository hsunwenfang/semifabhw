
#include <cstring>
#include <iostream>

constexpr int N = 1000000;

class Buffer{
    int* arr;
public:
    Buffer(){
        arr = new int[N];
    }
    // copy constructor — deep copy
    Buffer(const Buffer& other){
        arr = new int[N];
        std::memcpy(arr, other.arr, N * sizeof(int));
    }
    // move constructor — steal pointer
    Buffer(Buffer&& other) noexcept {
        arr = other.arr;
        other.arr = nullptr;
    }
    ~Buffer(){
        delete[] arr;
    }
};


int main() {
    Buffer old;
    Buffer copied = old;
    Buffer moved = std::move(old);
    return 0;
}