

#include <string>
#include <iostream>

template<typename T>
void print_twice(T& obj){
    std::cout << obj.to_string() << std::endl;
    std::cout << obj.to_string() << std::endl;
}

class A{
public:
    std::string to_string(){return "A";}
};

class B{
public:
    std::string to_string(){return "B";}
};

int main() {
    A a;B b;
    print_twice(a);
    print_twice(b);
    return 0;
}