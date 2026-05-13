

#include <iostream>

template <typename Derived>
class ISensor{
public:
    ISensor() = default;
    virtual ~ISensor() = default;
    double log(){
        return static_cast<Derived&>(*this).read();
    }
};

class TempSensor : public ISensor<TempSensor>{
public:
    double read() {return 35.5;}
};

class MockSensor : public ISensor<MockSensor>{
public:
    double read() {return 0.0;}
};


template <typename Derived>
void log_reading(ISensor<Derived>& s){std::cout << s.log() << std::endl;}

int main() {

    TempSensor ts;
    MockSensor ms;
    log_reading(ts);
    log_reading(ms);

    return 0;
}