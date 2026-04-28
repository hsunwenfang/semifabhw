#include "hal.h"
#include "data.h"
#include "logger.h"
#include "chamber.h"
#include "controller.h"
#include <iostream>
#include <csignal>
#include <atomic>

// 5-ISR
// std::atomic<bool> running=true;
volatile sig_atomic_t running=1;
void on_sigint(int){
    running = 0;
}

int main(){

    // signal listener
    signal(SIGINT, on_sigint);
    constexpr data::TimeRep CYCLE_US = 10000;
    // memory-mapped I/O register stolling here w/o true HW
    constexpr uint32_t GPIOA_ODR = 0x40020014;
    Stm32Gpio stm32u(&Reg<GPIOA_ODR>::ref(), 5);
    Chamber ch;
    Controller ctl;
    for (int i=0;i<5;i++){
    // while(true){
        if (!running) {break;}
        auto t0 = hal::micros();
        data::TempC reading = ch.measure();
        // pid_compute(); [TODO]
        data::TempC adj = ctl.update_compute(reading);
        // actuator_write();
        ch.adjust(adj);
        // non-blocking commutes with component like HMI
        // comms_poll();
        std::cout << ch.measure() << std::endl;
        // fulfil CYCLE_US for pid control
        data::TimeRep elapsed = hal::micros() - t0;
        if (elapsed < CYCLE_US)
            hal::delay_us(CYCLE_US - elapsed);
    }
    Logger::log();

}