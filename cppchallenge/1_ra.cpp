

#include <cstdint>

struct Pin{
    explicit Pin(uint8_t pin_num_) : pin_num(pin_num_){};
    ~Pin() = default;
    uint8_t pin_num;
};

template<uint32_t Addr>
class Reg{
    static volatile uint32_t& ref() { // ref() avoid instantiation
        return *reinterpret_cast<volatile uint32_t*>(Addr);
    }
public:
    Reg() = default;
    static void set_bit(uint32_t mask){ref() |= mask;}
    static void clear_bit(uint32_t mask){ref() &= ~mask;}
    static void set_pin(Pin pin){ref() |= (1 << pin.pin_num);}
};