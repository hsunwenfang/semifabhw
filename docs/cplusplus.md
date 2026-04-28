


# Types and Auto

cast

Cast	Safety	Runtime cost	Your project uses
static_cast	Compiler-checked	Zero	CRTP, enum conversion
dynamic_cast	Runtime-checked	vtable lookup	Not used (avoid in embedded)
reinterpret_cast	Unchecked	Zero	Reg<Addr> — int → pointer
const_cast	Unchecked	Zero	Not used

## rvalue and lvalue

- lvalue has a persistent address in mem so can be pointed to
- rvalue is only temporary exists during evaluation
- & test : lvalue has address while rvalue doesnot
- T& -> lvalue reference -> alias to variable
- const T& -> const lvalue reference applicable for rvalue -> read-only alias
- T&& -> rvalue referene -> take ownership of temporaries
[TODO] This matters less in embedded (you avoid heap/vectors), but the lvalue/rvalue distinction is why std::move exists — it casts an lvalue to an rvalue reference, saying "I'm done with this, steal its guts."

## function pointer

```cpp
/*
 * For historical reasons; programs expect signal's return value to be
 * defined by <sys/signal.h>.
 */
__BEGIN_DECLS
void(*signal(int, void (*)(int)))(int);
__END_DECLS
#endif  /* !_SYS_SIGNAL_H_ */
```
- void(*)(int) - pointer to a fn taking int returns void -> fnn
- signal is inputted int & fnn and output fnn

# Polymorphism

class Stm32Gpio : public IGpio {
- public : IGpio& ref = stm32_obj; -> private doesnot allow this

## class, struct and namespace

### 

- namespace can be extended anywhere so,
- namespace cannot have `template` not like the locally closed struct

```cpp
template<uint32_t Addr>
struct Reg {
    static volatile uint32_t& ref() {
        return *reinterpret_cast<volatile uint32_t*>(Addr);
    }
    static void set_bits(uint32_t mask) { ref() |= mask; }
    static void clear_bits(uint32_t mask) { ref() &= ~mask; }
};

// Usage
constexpr uint32_t GPIOA_ODR = 0x40020014;
Reg<GPIOA_ODR>::set_bits(1 << 5);  // set pin 5 high

Address        Byte
0x40020014  →  byte 0 (bits 0-7)    ← pins 0-7
0x40020015  →  byte 1 (bits 8-15)   ← pins 8-15
0x40020016  →  byte 2 (bits 16-23)  ← pins 16-23
0x40020017  →  byte 3 (bits 24-31)  ← pins 24-31
```

## virtual and override

- virtual prefixed parent method should be overwrite by child method
- virtual uint64_t micros() = 0; -> `=0` means pure virtual where base cannot provide implementation
- overwrite prefixed child method let compiler safety check
    - w/o overwrite may cause wrong parameter
- compliler stores virtual parent fns in vtable
- Cost : child fns points to the parent fns in vtable + indirect call when virtual function call

## dependency injection -> polymorphism at consumer

```cpp
// a specific address
constexpr uint32_t GPIOA_ODR = 0x40020014;
Stm32Gpio led(GPIOA_ODR, 5);
```
```cpp
// when Stm32Gpio is the only child
void blink(Stm32Gpio& led) {led.set(true); }
// 1 rutime polymorphism
// vtable lookup, works with any GPIO
void blink(IGpio& led) {led.set(true); }
```

## compile time polymorphism

```cpp
// 2 duck-typing compile time polymorphism
// no vtable -> can be constexpr & no base class
// but keep one fns copy per type and no mixed typed arr
template<typename Gpio>
void blink(Gpio& led) {led.set(true); }

// 3 CRTP
template<typename Derived>
class GpioBase {
public:
    // toggle body is identical for all derived
    void toggle() {
        // cast base [this] -> derived [self]
        auto& self = static_cast<Derived&>(*this);
        self.set(!self.read());
    }
};

class Stm32Gpio : public GpioBase<Stm32Gpio> {
public:
    void set(bool high) { /* register write */ }
    bool read() { return /* register read */; }
    // toggle() inherited from GpioBase — calls Stm32Gpio::set/read directly
};
```

## 

# Memory Structure

Full address space with bus routing:

0xFFFFFFFF ┌──────────────────────────┐
           │ System / Debug peripherals │ → routed to debug hardware
0xE0000000 ├──────────────────────────┤
           │                          │
           │     (reserved/unused)    │ → bus fault if accessed
           │                          │
0x60000000 ├──────────────────────────┤
           │   External memory        │ → routed to external bus pins
0x40020014 ├──────────────────────────┤
           │   Peripheral registers   │ → routed to HARDWARE LOGIC
           │   GPIO, UART, SPI, I2C   │   (transistors, flip-flops,
           │   ADC, DAC, Timers       │    comparators, shift registers)
           │                          │   NOT memory cells
0x40000000 ├──────────────────────────┤
           │                          │
           │   SRAM                   │ → routed to MEMORY CELLS
           │   ┌── Stack (top) ──┐    │   (actual 6-transistor SRAM cells
           │   │  grows down ↓   │    │    that store 0 or 1)
           │   ├─────────────────┤    │
           │   │  ↑ grows up     │    │
           │   │  Heap (bottom)  │    │
           │   ├─────────────────┤    │
           │   │  .bss (globals) │    │
           │   │  .data (init'd) │    │
           │   └─────────────────┘    │
0x20000000 ├──────────────────────────┤
           │   Flash (read-only)      │ → routed to flash memory cells
           │   your compiled code     │   (floating-gate transistors)
           │   const data             │
0x00000000 └──────────────────────────┘

## Heap and Queue

┌─────────────────────────┐  High address
│         Stack            │  ← grows downward
│  (automatic, per-function)│
│  local variables, args   │
│  freed when function returns│
├─────────────────────────┤
│           ↓              │
│       (free space)       │
│           ↑              │
├─────────────────────────┤
│         Heap             │  ← grows upward
│  (manual, lives until    │
│   you free it)           │
├─────────────────────────┤
│   Global/Static data     │
├─────────────────────────┤
│   Code (.text)           │
└─────────────────────────┘  Low address

## Stack v.s. Heap
- Allocate
    - Compiler automatically
    - You, via new / malloc
- Freed by
    - Automatically when function returns
    - delete / free
- Speed
    - 1 instruction (sub sp, #size)
    - Hundreds of instructions (find free block, update bookkeeping)
- Size
    - OS decide to have small stack and large heap
- Fragmentation
    - Never
    - repeated alloc/free leaves gaps

Scenario	Solution	Where
Size known at compile time	double buf[256]	Stack
Size known at runtime, small	alloca(n) (non-standard)	Stack — but risky
Size known at runtime, safe	std::vector<double> v(n)	Heap internally
Size bounded but varies	std::array<double, MAX_N>	Stack — allocate worst case

## SRAM vs GPIO ODR

- HW pretends to be memory [TODO]

Write to 0x20000004 (SRAM):
  CPU → bus → SRAM cell → stores bits in transistors
  Read back → same bits you wrote
  It's MEMORY — passive storage

Write to 0x40020014 (GPIO ODR):
  CPU → bus → GPIO peripheral → drives electrical pins HIGH/LOW
  Read back → current pin states (might differ from what you wrote!)
  It's HARDWARE — active logic that DOES things

## new

- calls `malloc` to grab heap memory
- calls the constructor on the memory when `pin` itself on the stack
- memory leak without `delete heap`
- avoided as heap is involved

## ref

- a compile time reference for the object

# C++ build pipeline

Source Code          Stage 1          Stage 2          Stage 3          Stage 4
   .cpp      ──────► .i/ii   ──────► .s       ──────► .o       ──────► executable
              PREPROCESS       COMPILE          ASSEMBLE          LINK

  main.cpp    main.ii          main.s           main.o           cvd_controller
  (text)      (expanded text)  (assembly text)  (machine code)   (runnable binary)

## 1. Preprocess cpp

- #include parts of .cpp are expanded by including .h into .ii

## 2. Complie cc1plus

- Lexing & Parser
    - Tokenize source → build Abstract Syntax Tree (AST)
    - tree structure reveals the sequence for execution
- Semantic analysis
    - type checking, access control
    - Verify pkt.temperature is double, send() args match signature
- Template instantiation
    - replace the `template <typename T>` arg
- Overload resolution
    - std::cout << pkt.temperature → pick operator<<(double)
- Optimization (-O2)
    - Inline functions, eliminate dead code, unroll loops, vectorize
- Register allocation
    - Map variables to CPU registers (rax, xmm0, etc.)
- Instruction selection
    - Choose x86 instructions (movsd, addsd, subsd)
- Struct layout
    - Compute field offsets: temperature is at byte 12 from struct start
- Stack frame layout
    - Decide where each local variable lives relative to rsp

### inline and constexpr

- inline in c++ stays inline in assembly achieves cross boundary optimization
- inline avoid the fn-calling bl (branch-and-link) asembly intruction overhead
- why not all inline
    - too much inline expand the file size so it failover to the next slower cache
- g++ -O2 will inline small fns without `inline` keyword
- Core meaning
    - runtime selection of compiler on which definition
    - "Can be evaluated at compile time"
- Compiler
    - `-O2` inlines small fns automatically
    - folds obvious compile-time const
- Edge

### const mutable volatile atomic

- `const` fns donot change the obj
- `mutable` var of an obj can change under `const` fns
    - [TODO] caching, logging, counters, mutexes
- `volatile` Inform compiler this variable can change outside your view
    - memory-mapped HW registers
    - signal modifing
- std::atomic<T> adds thread-safe read/write + memory ordering
- atomic is a single CPU instru `dmb  ish` over the complex std::mutex
    - atomic is lock_free while mutex is not
    - dmb  ish  drains all pending write before continue
- atomic<BigStruct> falls back to mutex for speed

## 3. Assemble as

## 4. Link ld

Linker resolves symbols by name — it does not re-check types.
If two .o files were compiled with different versions of the same .h
you get ODR violations (undefined behavior, no error).


## This project cpp run

cd /Users/hsunwenfang/Documents/semifabhw

# Stage 1: PREPROCESS — cpp (or g++ -E)
# CMake's target_include_directories becomes the -I flag
cpp -I src/common src/controller/main.cpp -o main.ii
# or equivalently:
g++ -E -I src/common src/controller/main.cpp -o main.ii
# → main.ii has ~50,000 lines (all headers expanded)

# Stage 2: COMPILE — g++ -S
g++ -S -std=c++17 -I src/common src/controller/main.cpp -o main.s
# → main.s is x86 assembly text

# Stage 3: ASSEMBLE — g++ -c (or 'as')
g++ -c -std=c++17 -I src/common src/controller/main.cpp -o main.o
# → main.o is machine code with unresolved symbols

# Stage 4: LINK — g++ (calls ld internally)
g++ main.o -o cvd_controller
# → cvd_controller is the runnable binary

# Or all at once (what CMake actually generates):
g++ -std=c++17 -I src/common src/controller/main.cpp -o cvd_controller


## g++ invoke binaries

g++ -v -std=c++17 -I src/common src/controller/main.cpp -o main 2>&1
Apple clang version 17.0.0 (clang-1700.6.3.2)
Target: arm64-apple-darwin25.4.0
Thread model: posix
InstalledDir: /Library/Developer/CommandLineTools/usr/bin
ignoring nonexistent directory "/Library/Developer/CommandLineTools/usr/bin/../include/c++/v1"

## -std=c++17 & POSIX & lib c

- std flag specifies which path stds should be found
- POSIX libs are OS specfic and can be checked by `g++ -###`
Aspect	std:: (C++)	libc (C)	POSIX
Defined by	ISO C++ committee	ISO C committee	IEEE / Open Group
Name mangling	Yes (_ZSt5clamp...)	No (memcpy)	No (signal)
Namespace	std::	std:: or global	Global only
Templates/overloads	Yes	No	No
Talks to kernel	Rarely	Sometimes (malloc)	Almost always
Portable to Windows	Yes (MSVC)	Yes (MSVC)	No

## struct and class

- Identical except struct is public by default and class is private


## Mangling and symbol

- One executable, one symbol

- Mangling distinguish the over loaded function name in C++
   _ZN3PID7computeEddd
    ││ │   │      ││││
    ││ │   │      │└─── d = double (paras)
    ││ │   │      └──── E = end of nested name
    ││ │   └──────────── 7compute = method name
    ││ └──────────────── 3PID = class name
    │└────────────────── N = nested name start
    └─────────────────── _Z = C++ mangled
- `extern "C" void handle_signal(int sig);` disable mangling so the symbol is `handle_signal` only
- `nm main.o`
    - T Your functions defined here:
    - t Static/inlined functions (local):
    - b Static local data:
    - r String literals and constants:
    - U Functions your code calls but doesn't define:
    - W Weak symbols (compiler-generated):
- weak symbols
    - inline funtions are intrinsically weak
    - compiler randomly keep one of the multiple conflicting weak symbols

## Headers file

- <T> must be in .h as they are weak and unimplemented
- one .h for all .cpp can avoid ODR (One Definition Rule)
    - Bazel / CMake guarantees single header version

### unity build

### __restrict 

- these pointers donot reference one another during function call -> compiler friendly

int		getaddrinfo(const char * __restrict, const char * __restrict,
			    const struct addrinfo * __restrict,
			    struct addrinfo ** __restrict);
