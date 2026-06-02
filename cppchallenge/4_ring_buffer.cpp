#include <cstddef>

template<typename T, size_t N>
class RingBuffer{
public:
    RingBuffer(){
        static_assert((N & (N-1)) == 0, "N must be power of 2");
    }
    size_t next(size_t s){ return (s+1) & (N-1); }
    size_t tail = 0;
    size_t head = 0;
    T buf[N];
    bool is_full(){ return next(head) == tail; }
    bool is_empty(){ return head == tail; }
};

template<typename T, size_t N>
class Producer{
public:
    void write(RingBuffer<T, N>& buffer, T data){
        if (buffer.is_full()) return;
        buffer.buf[buffer.head] = data;
        buffer.head = buffer.next(buffer.head);
    }
};

template<typename T, size_t N>
class Consumer{
public:
    T average(RingBuffer<T, N>& buffer){
        if (buffer.is_empty()) return T{};
        int count = 0;
        T sum = T{};
        for (size_t i = buffer.tail; i != buffer.head; i = buffer.next(i)){
            sum += buffer.buf[i];
            count++;
        }
        return sum / count;
    }
};
