

#include <vector>
#include <iostream>

int main() {
    std::vector<int> v(1000000, 0);
    std::vector<int> v2 = std::move(v);
    
    std::cout << v.size() << std::endl;
    std::cout << v2.size() << std::endl;
    
    return 0;
}