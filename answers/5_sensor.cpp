
#include <iostream>

class ISensor{
public:
    virtual ~ISensor() = default;
    virtual double read() = 0;
};

class TempSensor : public ISensor{
    double read () override{return 35.5;}
};

class MockSensor : public ISensor{
    double read() override{return 0.0;}
};


void log_reading(ISensor& s){std::cout << s.read() << std::endl;}

int main() {

    TempSensor ts;
    MockSensor ms;
    log_reading(ts);
    log_reading(ms);

    return 0;
}