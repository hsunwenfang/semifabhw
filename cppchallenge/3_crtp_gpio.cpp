#include <cstdint>

// Mock IRQ control for desktop testing
inline void __disable_irq(){}
inline void __enable_irq(){}

class CriticalSection{
public:
    CriticalSection(){ __disable_irq(); }
    ~CriticalSection(){ __enable_irq(); }
};

template<typename Derived>
class GpioBase{
protected:
    ~GpioBase() = default; // protected: prevents delete-through-base
public:
    GpioBase() = default;
    void toggle(){
        CriticalSection cs;
        Derived& self = static_cast<Derived&>(*this);
        self.set(!self.read());
    }
};

class MockGpio : public GpioBase<MockGpio>{
    bool state = false;
public:
    MockGpio() = default;
    ~MockGpio() = default;
    explicit MockGpio(bool v) : state(v) {};
    void set(bool v){state = v;}
    bool read(){return state;}
};
