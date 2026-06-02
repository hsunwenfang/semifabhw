

enum struct Err{OK, Error};

template <typename T>
class Result{
public:
    Result(T&& val, Err e) : value(val), err(e) {}
    Result(const T& val, Err e) : value(val), err(e) {}
    T value;
    Err err;
};

