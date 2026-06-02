# C++ Code Challenges

20 challenges covering concepts from cplusplus.md. Each challenge combines 3+ concepts.

---

## 1. Register Abstraction with Type-Safe Units

**Concepts**: value template, reinterpret_cast, type-safe units (strong typedef)

Write a `Reg<uint32_t Addr>` struct with `set_bits` / `clear_bits`. Add a `Pin` struct wrapping `uint8_t` so that `set_pin(Pin p)` cannot accept a raw integer. Demonstrate setting pin 5 high on address `0x40020014`.

```cpp
// Expected interface:
constexpr uint32_t GPIOA_ODR = 0x40020014;
Reg<GPIOA_ODR>::set_pin(Pin{5});  // compiles
// Reg<GPIOA_ODR>::set_pin(5);    // must NOT compile
```

---

## 2. Move-Only DMA Buffer

**Concepts**: move semantics (T&&), deleted copy constructor, placement new

Implement a `DmaBuffer` class that owns a fixed-size aligned buffer. Delete the copy constructor/assignment. Implement move constructor that steals ownership and nulls the source. Use placement new to construct a `Packet` inside the buffer.
[TODO] understand placement new and 

```cpp
// Expected:
DmaBuffer a;
a.construct<Packet>({.id=1, .temp=36.5});
DmaBuffer b = std::move(a);  // OK
// DmaBuffer c = b;           // must NOT compile
```

---

## 3. CRTP GPIO with RAII Lock

**Concepts**: CRTP, static_cast, RAII scope guard, destructor

Implement `GpioBase<Derived>` with a `toggle()` that calls `Derived::set()` and `Derived::read()`. Add a `CriticalSection` RAII (Resource Acquisition Is Intitialization) guard that disables/enables interrupts (simulate with a global counter). Ensure `toggle()` is atomic by constructing a `CriticalSection` inside it.

```cpp
struct MockGpio : GpioBase<MockGpio> {
    bool state = false;
    void set(bool v) { state = v; }
    bool read() { return state; }
};
```

---

## 4. ISR Ring Buffer with Volatile Semantics

**Concepts**: ring buffer, volatile, ISR communication, static_assert

Implement a power-of-2 `RingBuffer<T, N>` with `static_assert` verifying N is a power of 2. Mark `head` volatile (producer = ISR) and `tail` volatile (consumer = main). Write a simulated ISR that pushes ADC readings and a main loop that pops and averages them.

```cpp
RingBuffer<uint16_t, 64> adc_ring;
// simulate: ISR pushes, main pops
```

---

## 5. Cooperative Scheduler with Function Pointers

**Concepts**: function pointer, std::chrono time_point/duration, class template

Build a `Task<T>` class templated on a context type. Store a `void(*)(T&)` callback, a `chrono::milliseconds` period, and a `time_point` for last execution. Implement a `Scheduler::tick()` that iterates tasks and runs due ones.

```cpp
struct SensorCtx { int reading; };
void read_sensor(SensorCtx& ctx) { ctx.reading++; }
Task<SensorCtx> t{read_sensor, 100ms};
```

---

## 6. Polymorphic Error Result without Exceptions

**Concepts**: error handling (Result type), explicit operator bool, copy vs move

Implement `Result<T>` with a value and an `Err` enum. Implement both copy and move constructors.
The copy must deep-copy the value. The move must steal.
Add `explicit operator bool()` that returns true on `Err::Ok`. Show that moving a `Result<std::vector<int>>` is O(1) while copying is O(n).

```cpp
Result<std::vector<int>> ok({1,2,3}, Err::Ok);
auto moved = std::move(ok);   // O(1)
auto copied = moved;          // O(n)
```

---

## 7. State Machine with enum class and constexpr Transitions

**Concepts**: enum class, static_cast<int>, constexpr, state machine pattern

Define `State` and `Event` enums. Build a `constexpr` transition table as a 2D array indexed by `static_cast<int>(state)` and `static_cast<int>(event)`. Verify at compile time with `static_assert` that `transition(Idle, Start) == Heating`.

```cpp
constexpr State table[5][5] = { /* ... */ };
static_assert(table[static_cast<int>(State::Idle)][static_cast<int>(Event::Start)] == State::Heating);
```

---

## 8. Pool Allocator with Trivial Type Check

**Concepts**: pool allocation, std::is_trivially_copyable, reinterpret_cast, aligned_storage

Implement `Pool<T, N>` that only compiles for trivially copyable types (use `static_assert`). Use `reinterpret_cast` to convert storage slots to `T*`. Demonstrate that `Pool<std::string, 8>` fails to compile.

```cpp
Pool<int, 16> int_pool;       // OK
// Pool<std::string, 16> sp;  // static_assert fires
```

---

## 9. Virtual Interface with vtable Cost Measurement

**Concepts**: virtual/override, vtable indirect call, dependency injection, inline

Define `ISensor` with a pure virtual `read()`. Derive `AdcSensor`. Write two versions of `sample()`: one taking `ISensor&` (virtual dispatch) and one as a template (duck-typing, inlined). Use `chrono` to measure the overhead difference in a tight loop.

```cpp
void sample_virtual(ISensor& s);      // vtable lookup
template<typename S>
void sample_duck(S& s);               // inlined, no vtable
```

---

## 10. Header-Only Library with ODR Safety

**Concepts**: inline, ODR, weak symbols, header guards, constexpr

Create a header `math_utils.h` with:
- An `inline` function `lerp()`
- A `constexpr` function `clamp()`
- A non-inline function (intentionally broken)

Include it from two separate .cpp files. Explain (in comments) why the non-inline version causes an ODR violation and how `nm` would show duplicate `T` symbols vs `W` symbols.

---

## 11. Double Buffer with Atomic Swap

**Concepts**: double buffering, std::atomic, volatile, extern "C"

Implement a double buffer with `std::atomic<int> active`. The ISR (marked `extern "C"`) swaps the active index atomically. The main thread processes the inactive buffer. Explain why `volatile` alone is insufficient (no atomicity guarantee) and why `atomic` is needed.

```cpp
alignas(32) uint16_t buf[2][128];
std::atomic<int> active{0};
extern "C" void DMA_IRQHandler() { active.fetch_xor(1); }
```

---

## 12. extern "C" Shared Library with Name Mangling

**Concepts**: extern "C", name mangling, .so/.dylib, function pointer

Write a small "plugin" interface: a header declaring `extern "C" Plugin* create_plugin()`. Implement two plugins in separate files. Show the mangled vs unmangled symbols using `nm` output format. Load via a function pointer simulating `dlopen`/`dlsym`.

```cpp
// plugin.h
extern "C" Plugin* create_plugin();
// nm output: T create_plugin   (no mangling)
// vs C++:    T _ZN6Plugin6createEv
```

---

## 13. Scope Guard + Move Semantics for SPI Transaction

**Concepts**: RAII scope guard, move constructor, deleted copy, destructor

Implement `SpiTransaction` that asserts CS low on construction, CS high on destruction. Delete copy (two objects releasing the same CS = double free analog). Implement move (transfers ownership). Show that returning from a function uses move, not copy.

```cpp
SpiTransaction begin_transfer(SpiPort& port) {
    return SpiTransaction(port);  // move, not copy
}
```

---

## 14. constexpr Lookup Table with static_assert Validation

**Concepts**: constexpr, static_assert, value template, array initialization

Build a `constexpr` CRC8 lookup table (256 entries) computed entirely at compile time. Use `static_assert` to verify known CRC values. Template the polynomial as a value parameter `template<uint8_t Poly>`.

```cpp
template<uint8_t Poly>
struct Crc8 {
    static constexpr auto table = make_table();
    static constexpr uint8_t compute(const uint8_t* data, size_t len);
};
static_assert(Crc8<0x07>::compute(...) == 0xBC);
```

---

## 15. lvalue/rvalue Overload Set for Zero-Copy Queue

**Concepts**: T& vs T&&, const T&, move semantics, ring buffer

Implement a `Queue<T>` with two `push` overloads:
- `push(const T&)` — copies into buffer
- `push(T&&)` — moves into buffer

Demonstrate that pushing a temporary `std::vector` uses the move overload (no heap allocation for the copy), while pushing a named variable uses the copy overload.

```cpp
Queue<std::vector<int>> q;
std::vector<int> v = {1,2,3};
q.push(v);              // copy
q.push(std::move(v));   // move — v is now empty
q.push({4,5,6});        // move (rvalue temporary)
```

---

## 16. Compile-Time Platform GPIO with static Dispatch

**Concepts**: template specialization, static class member, using alias, compile-time platform selection

Define `template<typename MCU> struct Gpio;` with specializations for `Stm32F4` and `NrfGpio`. Each has `static void set(int pin)` and `static bool read(int pin)`. Use a `using` alias to select platform at one point. Show that no vtable exists (`nm` shows `T`, not indirect dispatch).

```cpp
#ifdef TARGET_STM32
using Platform = Stm32F4;
#else
using Platform = NrfGpio;
#endif
using Led = Gpio<Platform>;
Led::set(5);
```

---

## 17. Mutable Cache in const Method with volatile Hardware Read

**Concepts**: mutable, const method, volatile, reinterpret_cast

Implement a `Sensor` class with a `mutable` cache field and a `const` method `read() const`. Inside `read()`, check if the cache is stale; if so, perform a `volatile` hardware register read via `reinterpret_cast`. Return the cached value. Explain why `mutable` is needed despite the method being `const`.

```cpp
class Sensor {
    mutable uint16_t cache_ = 0;
    mutable bool valid_ = false;
    static constexpr uint32_t REG_ADDR = 0x40001000;
public:
    uint16_t read() const;  // const but updates mutable cache
};
```

---

## 18. Build Pipeline Verification Script

**Concepts**: preprocessor (-E), assembler (-S), object file (-c), nm symbols, linker

Write a multi-file C++ program (main.cpp + utils.cpp + utils.h). Provide shell commands that:
1. Preprocess main.cpp → show `#include` expansion
2. Compile to assembly → find the function labels
3. Assemble to .o → use `nm` to show U (undefined) symbols
4. Link → verify all U symbols are resolved

```sh
g++ -E main.cpp -o main.ii          # stage 1
g++ -S main.cpp -o main.s           # stage 2
g++ -c main.cpp -o main.o && nm main.o | grep " U "  # stage 3
g++ main.o utils.o -o app && nm app | grep " U "     # stage 4 (should be empty or only libc)
```

---

## 19. unique_ptr Custom Deleter for Memory-Mapped Peripheral

**Concepts**: std::unique_ptr, destructor/RAII, new/delete vs placement, static allocation

Implement a custom deleter `MmioDeleter` that doesn't call `delete` (because the peripheral memory is statically mapped). Use `std::unique_ptr<Peripheral, MmioDeleter>` to wrap a peripheral handle. Show that the destructor runs cleanup logic (e.g., disable clock) without freeing memory.

```cpp
struct MmioDeleter {
    void operator()(Peripheral* p) {
        p->CR &= ~ENABLE_BIT;  // disable, but don't free — it's MMIO
    }
};
using PeriphHandle = std::unique_ptr<Peripheral, MmioDeleter>;
```

---

## 20. __restrict Optimized DSP Filter with Placement New Output

**Concepts**: __restrict, placement new, aligned storage, trivially copyable

Implement an FIR filter function with `__restrict` qualified input/output pointers (no aliasing). Allocate the output buffer using `alignas(32)` static storage and construct via placement new. Use `static_assert` to verify the sample type is trivially copyable (safe for DMA transfer).

```cpp
void fir_filter(
    const float* __restrict input,
    float* __restrict output,
    const float* __restrict coeffs,
    size_t n_taps, size_t n_samples);

alignas(32) static uint8_t out_storage[sizeof(float) * 256];
float* out = new (out_storage) float[256];  // placement new, no malloc
static_assert(std::is_trivially_copyable_v<float>);
```

---

## Concept Coverage Matrix

| # | Concepts |
|---|---|
| 1 | value template, reinterpret_cast, type-safe units |
| 2 | move (T&&), deleted copy ctor, placement new |
| 3 | CRTP, static_cast, RAII scope guard |
| 4 | ring buffer, volatile, ISR comm, static_assert |
| 5 | function pointer, chrono, class template |
| 6 | Result/error handling, operator bool, copy vs move |
| 7 | enum class, static_cast<int>, constexpr, state machine |
| 8 | pool allocation, is_trivially_copyable, reinterpret_cast |
| 9 | virtual/override, vtable cost, dependency injection, inline |
| 10 | inline, ODR, weak symbols, header guards, constexpr |
| 11 | double buffering, std::atomic, volatile, extern "C" |
| 12 | extern "C", name mangling, .so/.dylib, function pointer |
| 13 | RAII, move ctor, deleted copy, destructor |
| 14 | constexpr, static_assert, value template |
| 15 | T& vs T&&, const T&, move semantics, ring buffer |
| 16 | template specialization, static member, using alias |
| 17 | mutable, const method, volatile, reinterpret_cast |
| 18 | preprocess, assemble, nm symbols, linker |
| 19 | unique_ptr, RAII/destructor, placement/static alloc |
| 20 | __restrict, placement new, aligned storage, trivially copyable |
