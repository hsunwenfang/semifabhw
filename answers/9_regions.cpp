

#include <iostream>

int main() {
    int i1 = 5;
    int* ip2 = new int(6);
    static int i3 = 7;
    std::cout << &i1 << std::endl;
    std::cout << ip2 << std::endl;
    std::cout << &i3 << std::endl;
    return 0;
}