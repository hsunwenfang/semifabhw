

#include <cstdint>
#include <iostream>
// "" goes project dir first while <> goes system first
#include "hal.h"

int main(){
    constexpr uint32_t GPIOA_ODR = 0x40020014;
    // Stm32Gpio& stm32u = Stm32Gpio(Reg<GPIOA_ODR>::ref(), 5);
    Stm32Gpio stm32u(&Reg<GPIOA_ODR>::ref(), 5);
    // std::cout << stm32u.pin_ << std::endl;
    
    return 0;
}