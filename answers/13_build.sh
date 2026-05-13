

# preprocess
g++ -std=c++17 -E -o math.ii math.cpp
# compile to assembly
g++ -std=c++17 -S -o math.s math.cpp
# compile to executable
g++ -std=c++17 -shared -fPIC -o math.dylib math.cpp