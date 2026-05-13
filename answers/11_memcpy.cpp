
#include <cstring>
#include <vector>

void destroy_smart_container(){
    std::vector<int> v1(10,0);
    std::vector<int> v2;
    std::memcpy(v2, v1, sizeof(std::vector<int>));
}

int main(){
    destroy_smart_container();
    return 0;
}