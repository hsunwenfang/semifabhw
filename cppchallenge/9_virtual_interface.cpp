
class ISensor {
public:
    virtual ~ISensor() = default;
    virtual int read() = 0;
};

class AdcSensor : public ISensor {
public:
    int read() override { return 35; }
};

// Virtual dispatch version
int sample_virtual(ISensor& s) { return s.read(); }

// Duck-typing version (inlined, no vtable)
template<typename S>
int sample_duck(S& s) { return s.read(); }