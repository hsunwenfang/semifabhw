

#include "data.h"
#include <random>

class Chamber{
    data::TempC tem = 15.0;
    // seed generating engine
    std::mt19937 gen_{55};
public:
    Chamber() = default;
    ~Chamber() = default;
    inline data::TempC adjust(data::TempC adj){
        tem += adj;
        return tem;
    }
    inline data::TempC measure(){
        std::normal_distribution<data::TempC> temp_distri(tem, 1.0);
        data::TempC sample = temp_distri(gen_);
        
        return std::max(0.0, sample);
    }
};