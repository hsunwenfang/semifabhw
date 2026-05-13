

#include <iostream>

using namespace std;

enum class Color {Red, Green, Blue};

int main(){
    cout << static_cast<int>(Color::Red) << endl;
    cout << static_cast<int>(Color::Green) << endl;
    cout << static_cast<int>(Color::Blue) << endl;
    return 0;
}