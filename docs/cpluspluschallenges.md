# C++ Concept Challenges

Answer scripts go in the `answers/` directory, named like `1_enum.cpp`, `2_addr.cpp`, etc.
Run `g++ -std=c++17 challenges_test.cpp -o challenges_test && ./challenges_test` to test all answers.

## Types and Casts

Q1. Write a function that takes an `enum class Color { Red, Green, Blue }` and returns its underlying int value using the correct cast. → `answers/1_enum.cpp`

A1.

Q2. Given a `uint32_t addr = 0x40020014;`, write a one-liner that writes `0x20` to that hardware register address using the correct cast and `volatile`. → `answers/2_addr.cpp`

A2.

[TODO] : value / pointer operations

## rvalue, lvalue, and move

Q3. Write a function `void process(int& x)` and `void process(int&& x)` that print "lvalue" or "rvalue". Call both from main to demonstrate each path. → `answers/3_rvaluelvalue.cpp`

[TODO] : is_lvalue_reference_v mechanism

Q4. Write code that creates a `std::vector<int>` with 1M elements, then transfers it to a second vector using `std::move`. Print the size of both vectors after the move. → `answers/4_move.cpp`

[TODO] : sizeof() and .size() and stack or heap

## Polymorphism

Q5. Write an abstract base class `ISensor` with a pure virtual `double read() = 0`. Write two derived classes `TempSensor` and `MockSensor`. Write a function `void log_reading(ISensor& s)` that prints the reading. Call it with both types. → `answers/5_sensor.cpp`

[TODO] `virtual` needed on destrutor and cannot be used on constructer

Q6. Rewrite Q5 using CRTP instead of virtual. The base `SensorBase<Derived>` should provide a `void log()` method that calls `self.read()` via `static_cast`. → `answers/6_crtp.cpp`

[TODO] base and derived classes holding different method name can avoid recursion issue

## Templates and duck-typing

Q7. Write a template function `template<typename T> void print_twice(T& obj)` that calls `obj.to_string()` twice. Write two unrelated structs (no base class) that each have `to_string()`. Demonstrate that both work. → `answers/7_duck.cpp`

A7.

## Memory layout

Q8. Write a struct `Packet { uint8_t id; double temperature; uint16_t crc; }`. Print `sizeof(Packet)` and use `offsetof` to show each field's offset. Explain the padding. → `answers/8_layout.cpp`

A8.

Q9. Write code that declares an `int` on the stack, a `new int` on the heap, and a `static int`. Print the address of each and verify they fall in different memory regions. → `answers/9_regions.cpp`

A9.

## Copy vs move

Q10. Write a class `Buffer` that owns a `new int[N]` array. Implement both the copy constructor (deep copy) and move constructor (steal pointer). Print messages in each to prove which is called. → `answers/10_buffer.cpp`

[TODO] assignment point to the pointer while memcpy shallow copy the stack data hence is risky of double reference
[TODO] std::move only tells the compiler to treat old an rvalue
[TODO] std::vector is a `smart container` managing deep copy while arr is a pure pointer replicate the pointer address.

Q11. Show what happens if you `memcpy` a `std::vector<int>` from `a` to `b` then destroy both. Explain (in a comment) why it crashes without running the Undefined Behavior. → `answers/11_memcpy.cpp`

A11.

## Smart pointers

Q12. Write a function that returns a `std::unique_ptr<ISensor>` (from Q5). The caller should call `read()` through the pointer. No `delete` anywhere. → `answers/12_smart.cpp`

[TODO] detailed illustrate heap / stack structure and how move / memcpy / reference / pointer / rvalue / lvalue and other related operations in this sheet leverage them

## Build pipeline

Q13. Given two files `math.h` (declares `int add(int,int)`) and `math.cpp` (defines it) and `main.cpp` (calls `add`), write the exact `g++` commands to: (a) preprocess main.cpp, (b) compile to .o, (c) link into executable. Three separate commands. → `answers/13_build.sh`

[TODO] tidy up build pipeline

## Linking and symbols

Q14. Write a `compute.cpp` with a function `double compute(double x)` and build it as a shared library `.dylib` with `extern "C"`. Write the `g++` command and then a Python script using `ctypes.CDLL` to call it. → `answers/14_compute.cpp`

A14.

## inline, constexpr, and ODR

Q15. Put a non-inline function definition `int helper() { return 42; }` in a `.h` file and include it from two `.cpp` files. Show the linker error. Then fix it with `inline`. → `answers/15_inline/`

A15.

Q16. Write a constexpr int factorial(int n) function. Show it being used at compile time (static_assert) and at runtime (with std::cin input). → `answers/16_factorial.cpp`

A16.

## const, volatile, mutable

Q17. Write a class `Cache` with a `const` method `int get(int key) const` that updates an internal `mutable std::map` hit counter. Demonstrate calling it on a `const Cache&`. → `answers/17_cache.cpp`

A17.

Q18. Write a `volatile uint32_t reg;` and a loop that reads it 100 times. Compile with `-O2` and show (via assembly or behavior) that the compiler does NOT optimize the reads away. Then remove `volatile` and show it does. → `answers/18_volatile.cpp`

A18.

## Header guards and ODR

Q19. Write a `config.h` without include guards that defines `const int MAX = 100;`. Include it from two places in one `.cpp` and show the compiler error. Fix it with `#ifndef` guards. → `answers/19_guards/`

[TODO] CONFIG_H is the filename

## struct vs class convention

Q20. Write a `struct Point { double x, y; }` (passive data) and a `class PidController` with private `kp_, ki_, kd_` and a public `double compute(double error)`. Demonstrate using both from `main`. → `answers/20_structclass.cpp`

A20.

