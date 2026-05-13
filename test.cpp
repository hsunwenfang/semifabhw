

#include <iostream>
using namespace std;

int main(){
    int x = 5;int& y = x;
    int* xp = &x;
    int* yp = &y;

    cout << x << "  " << y << "  " << xp << "  " << yp << endl;
    cout << &x << "  " << &y << "  " << &xp << "  " << &yp << endl;
    return 0;
}