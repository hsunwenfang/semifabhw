
#include "data.h"

class Controller{
    data::TempC temp_target = 15.0;
public:
    Controller() = default;
    ~Controller() = default;
    inline data::TempC update_compute(data::TempC& measured){
        return temp_target - measured;
    }
};