

#include <iostream>
using namespace std;

void process(int& v){
    cout << "lvalue" << endl;
}

void process(int&& v){
    cout << "rvalue" << endl;
}

int main() {
    int i = 5;
    process(i);
    process(5);
    return 0;
}