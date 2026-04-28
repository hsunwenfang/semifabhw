
#pragma once
#include <iostream>

class Logger{
    int verbosity;
public:
    inline static void log(){
        std::cout << "Logging" << std::endl;
    }
};