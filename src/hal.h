
// prevent multiple includes from gpio.h, uart.h ... files from different module
#pragma once
// timing
#include <chrono>
#include <cstdint>

namespace hal{
    // monotonic steady clock always goes forward
    // uint32_t is not enough for microseconds accumulator
    using namespace std::chrono;
    inline uint64_t micros() {
        return duration_cast<microseconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }
    inline void delay_us(uint32_t us){
        auto start = steady_clock::now();
        while (steady_clock::now() - start < microseconds(us))
        {
            // spin-wait : check hw status again
        }
        

    }
}

template<uint32_t Addr>
struct Reg{
    static volatile uint32_t& ref() {
        return *reinterpret_cast<volatile uint32_t*>(Addr);
    }
    static void set_bits(uint32_t mask){ ref() |= mask; }
    static void clear_bits(uint32_t mask){ ref() &= ~mask; }
};

class IGpio{
public:
    virtual ~IGpio() = default;
    virtual void set(bool high) = 0;
    virtual bool read() = 0;
};

// one pin for STMicroelectronics
class Stm32Gpio : public IGpio {
    volatile uint32_t* reg_;
    uint8_t pin_;
public:
    Stm32Gpio(volatile uint32_t* reg, uint8_t pin) : reg_(reg), pin_(pin) {}
    void set(bool high) override {};
    // right shift [pin_] to postion 0 
    // & 1 to eliminate leading terms
    bool read() override{ return (*reg_ >> pin_) & 1; };
};

// [TODO] use bitmask ops to operate on the full 32 bitarr
// class Stm32GpioPort{};