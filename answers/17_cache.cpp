
#include <map>
#include <iostream>

class Cache{

    mutable std::map<int, int> count;

public:
    ~Cache() = default;
    int get(int key) const{
        count[key]++;
        return key;
    }

    void print_count() const{
        for (const auto& [k, v] : count) {
            std::cout << k << "  " << v << std::endl;
        }
    }
};

int main() {
    Cache cache;
    const Cache& c = cache;
    for (int k=0; k<13; k++){
        c.get(k);
    }
    c.print_count();
    return 0;
}