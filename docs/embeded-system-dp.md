# Embedded System Design Patterns

#### No-OS internal (single MCU bugs)

| # | Problem | Root cause | Debug method |
|---|---|---|---|
| 1 | **Stack overflow** | Deep call chain / large local arrays | Canary pattern, check SP |
| 2 | **ISR ↔ main data tear** | Non-atomic read of multi-byte struct | Critical section or double-buffer |
| 3 | **Missed interrupts** | Critical section too long | Timer measure IRQ-disabled duration |
| 4 | **Timing jitter** | Super-loop iteration time varies | Hardware timer trigger, not software delay |
| 5 | **Watchdog reset loop** | One code path forgets to kick | Store state in backup register before reset |
| 6 | **Heisenbug** | Debug printf changes timing | GPIO toggle + scope (zero overhead) |

#### No-OS ↔ No-OS (sensor-to-sensor, rare)

| # | Problem | Root cause | Debug method |
|---|---|---|---|
| 7 | **CAN bus contention** | Two MCUs transmit simultaneously, lower-priority always loses | Check CAN priority assignment, add jitter |
| 8 | **Daisy-chain SPI desync** | One MCU resets, shifts all downstream data by N bits | Frame delimiter + CRC per device |
| 9 | **Shared sensor conflict** | Two MCUs try to read same I2C sensor | Bus arbitration failure → assign master |

#### RTOS ↔ No-OS (controller to sensor MCU)

| # | Problem | Root cause | Debug method |
|---|---|---|---|
| 10 | **Command/response mismatch** | RTOS sends next cmd before MCU finished previous | Sequence number protocol, wait-for-ACK |
| 11 | **I2C/SPI bus lockup** | MCU reset mid-transaction, slave holds bus | 9-clock recovery, bus timeout watchdog |
| 12 | **Sensor data stale** | MCU hung or crashed, RTOS reads old buffer | Timestamp in every packet, reject if too old |
| 13 | **Baud rate mismatch after update** | MCU firmware updated, RTOS not updated | Version handshake at boot |
| 14 | **DMA overrun** | RTOS polls too slow, MCU's DMA wraps around buffer | Flow control: MCU signals "buffer full" |

#### RTOS internal (single controller bugs)

| # | Problem | Root cause | Debug method |
|---|---|---|---|
| 15 | **Priority inversion** | Low-prio task holds mutex needed by high-prio | Priority inheritance, Tracealyzer |
| 16 | **Deadlock** | Task A waits on B's mutex, B waits on A's | Lock ordering convention, timeout on all mutexes |
| 17 | **Task starvation** | One task never yields, lower-prio tasks starve | Preemptive scheduler + time-slice for same-priority |
| 18 | **Memory leak (heap)** | Dynamic alloc in task without matching free | Pool allocator instead of malloc, track high-water |
| 19 | **Race in shared state** | Two tasks modify recipe state without lock | Mutex or message-passing (share nothing) |

#### RTOS ↔ RTOS (peer controller coordination)

| # | Problem | Root cause | Debug method |
|---|---|---|---|
| 20 | **Lost handoff message** | UDP dropped, no retry | Sequence + ACK + timeout + retry |
| 21 | **Split-brain** | Network partition, both think they're master | Heartbeat timeout → safe-state, single arbitrator |
| 22 | **Ordering violation** | Chamber B starts before transfer confirms placement | State machine enforces preconditions |
| 23 | **Clock drift** | Two boards disagree on timestamps → wrong sequence | PTP (IEEE 1588) or NTP sync, or use sequence numbers not time |
| 24 | **Reflective memory stale** | Writer crashes, reader sees old valid data | Generation counter + watchdog bit in shared mem |

#### Full-OS ↔ RTOS (equipment host to chamber)

| # | Problem | Root cause | Debug method |
|---|---|---|---|
| 25 | **Recipe step timeout** | RTOS stuck in fault state, host keeps waiting | Host-side watchdog per step with configurable timeout |
| 26 | **Parameter mismatch** | Host sends float, RTOS expects fixed-point | Interface definition (IDL), version in message header |
| 27 | **Network buffer overflow** | Host floods RTOS with commands faster than processed | Flow control: RTOS ACKs each command before next |
| 28 | **Log correlation** | Bug spans host + RTOS, timestamps don't match | Synchronized time (PTP), unified trace ID |

#### Full-OS ↔ Factory (SECS/GEM)

| # | Problem | Root cause | Debug method |
|---|---|---|---|
| 29 | **HSMS connection drop** | Network switch failover, tool goes offline | Auto-reconnect with state recovery |
| 30 | **Recipe download corrupt** | Large recipe > single message, assembly error | Chunked transfer + CRC + verify-after-write |

## Hardware Abstraction Patterns

> CPU registers, memory-mapped I/O, bus architecture → [embeded-system-hw.md § 1](embeded-system-hw.md#1-compute--cpu--registers)

### 1. Hardware Abstraction Layer (HAL) with polymorphism

```armasm
ldr  r0, [obj]         // load vptr from object (memory read #1)
ldr  r1, [r0, #4]     // load function address from vtable (memory read #2)
blx  r1               // indirect branch to unknown address
```

```cpp
class IGpio {
public:
    virtual ~IGpio() = default;
    virtual void set(bool high) = 0;
    virtual bool read() = 0;
};

class Stm32Gpio : public IGpio {
    volatile uint32_t* reg_;
    uint8_t pin_;
public:
    Stm32Gpio(volatile uint32_t* reg, uint8_t pin) : reg_(reg), pin_(pin) {}
    void set(bool high) override { /* write bit to register */ }
    bool read() override { return (*reg_ >> pin_) & 1; }
};
```

### 2. Register Access Pattern

Wrap memory-mapped I/O registers in typed structs with `volatile` pointer access.
Use `static_assert` on struct sizes to guarantee layout matches hardware documentation. 
`Reg<Addr>` template finishes at compile time

```cpp
template<uint32_t Addr>
struct Reg {
    static volatile uint32_t& ref() {
        return *reinterpret_cast<volatile uint32_t*>(Addr);
    }
    static void set_bits(uint32_t mask) { ref() |= mask; }
    static void clear_bits(uint32_t mask) { ref() &= ~mask; }
};

// constexpr is guranteed to be resolved at compile time -> can be templated
constexpr uint32_t GPIOA_ODR = 0x40020014;
Reg<GPIOA_ODR>::set_bits(1 << 5);  // set pin 5 high
// static_assert
static_assert(sizeof(GpioRegs) == 24, "GPIO register block size mismatch");
```

### 3. Board Support Package (BSP)

link only the wanted `bsp.h`/`bsp.cpp` pair per target board.

```bash
# For real hardware
target_sources(app PRIVATE bsp_nucleo.cpp)
# For Mac simulator
target_sources(app PRIVATE bsp_sim.cpp)
```

---

## Concurrency / Scheduling Patterns

> Interrupt hardware mechanics (NVIC, vector table, context save) → [embeded-system-hw.md § 2](embeded-system-hw.md#2-interrupt-hardware-nvic)

### 4. Super Loop

simple loop works well when all tasks fit within the cycle budget.
Add a cycle-time watchdog to detect overruns.
Your CVD controller uses this pattern — `recv()`, state machine, `send()` in a loop.

```cpp
int main() {
    hal::init();
    while (true) {
        auto t0 = hal::micros();
        sensor_read();
        pid_update();
        actuator_write();
        comms_poll();
        auto elapsed = hal::micros() - t0;
        if (elapsed < CYCLE_US)
            hal::delay_us(CYCLE_US - elapsed);
    }
}
```

### 5. Interrupt / ISR Pattern
### 6. Interrupt → Deferred Work

- Register a short function that hw invokes when an event occurs (pin change, timer tick, UART byte received).
- ISR must execute quickly — Avoid allocations, I/O, or long computation inside ISRs. -> hardwork deferred to SW callback
- use `volatile sig_atomic_t running=1;`
- On POSIX systems (macOS simulation), model interrupts using `signal()` or `timer_create()` with `SIGALRM`. 
- `mishandling` ISR/main boundary causes race conditions and data corruption.

### 7. Cooperative Scheduler

check one-by-one if `last_run > period_ms`

```cpp
struct Task {
    // function pointer named `run`
    void (*run)();
    uint32_t period_ms;
    uint32_t last_run;
};

Task tasks[] = {
    {sensor_read,   10,  0},   // 100 Hz
    {pid_update,    10,  0},   // 100 Hz
    {comms_poll,    50,  0},   // 20 Hz
    {display_update,200, 0},   // 5 Hz
};

void scheduler() {
    while (true) {
        uint32_t now = hal::millis();
        for (auto& t : tasks) {
            if (now - t.last_run >= t.period_ms) {
                t.run();
                t.last_run = now;
            }
        }
    }
}
```

### 8. Preemptive Scheduler (RTOS)

Interrupt in preemption can casue race condition or unrecoverable corrupted

### 9. Rate Monotonic Scheduling

the task with the shortest period gets the highest priority.
CPU utilization and calculate `wcet_us`

```cpp
// RMS: shortest period = highest priority
struct RmsTask {
    void (*run)();
    uint32_t period_us;
    uint32_t wcet_us;      // worst-case execution time
    uint32_t deadline_us;
};

// Verify schedulability: U = sum(wcet/period) < N*(2^(1/N) - 1)
bool is_schedulable(RmsTask* tasks, int n) {
    double u = 0;
    for (int i = 0; i < n; i++)
        u += (double)tasks[i].wcet_us / tasks[i].period_us;
    double bound = n * (std::pow(2.0, 1.0/n) - 1.0);
    return u <= bound;
}
```

---

## State Machine Patterns

### 10. Flat State Machine

A `switch` statement over an enum of states, executed each cycle.

```cpp
enum class State { IDLE, PUMP_DOWN, PROCESS, FAULT };
State state = State::IDLE;

void fsm_step(const SensorData& s) {
    State prev = state;
    switch (state) {
        case State::IDLE:
            if (start_requested) state = State::PUMP_DOWN;
            break;
        case State::PUMP_DOWN:
            set_throttle(100);
            if (s.pressure < 0.01) state = State::PROCESS;
            break;
        case State::PROCESS:
            if (s.temperature > 500) state = State::FAULT;
            break;
        case State::FAULT:
            all_outputs_safe();
            break;
    }
    if (state != prev) log_transition(prev, state);
}
```

### 11. State Table

`{current_state, event, guard, next_state, action}`.

```cpp
struct Transition {
    State from;
    Event event;
    bool (*guard)();
    State to;
    void (*action)();
};

const Transition table[] = {
    {State::IDLE,      Event::START,    nullptr,          State::PUMP_DOWN, start_pump},
    {State::PUMP_DOWN, Event::LOW_PRES, nullptr,          State::PROCESS,   begin_process},
    {State::PROCESS,   Event::TIMEOUT,  nullptr,          State::PURGE,     begin_purge},
    {State::PROCESS,   Event::OVERTEMP, is_temp_critical, State::FAULT,     emergency_stop},
};

void dispatch(Event e) {
    for (auto& t : table)
        if (t.from == state && t.event == e && (!t.guard || t.guard())) {
            if (t.action) t.action();
            state = t.to;
            return;
        }
}
```

### 13. State Pattern (OOP)

A base `IState` interface with `enter()`, `execute()`, `exit()` methods.
This separates per-state logic into dedicated classes, each testable in isolation.
Downside: heap allocation or a static pool is needed for state objects, and the indirection can be too heavy for small MCUs.

```cpp
class IState {
public:
    virtual ~IState() = default;
    virtual void enter() = 0;
    virtual void execute(Context& ctx) = 0;
    virtual void exit() = 0;
};

class PumpDownState : public IState {
public:
    void enter() override { log("entering PUMP_DOWN"); }
    void execute(Context& ctx) override {
        ctx.set_throttle(100);
        if (ctx.pressure() < 0.01)
            ctx.transition_to(&ctx.gas_stabilize_state);
    }
    void exit() override { log("leaving PUMP_DOWN"); }
};
```

---

## Communication Patterns

### 14. Observer / Publish-Subscribe

A subject maintains a list of callback function pointers.
When an event occurs, it iterates the list and invokes each callback with the event data.
Subscribers register/unregister at runtime.

Use a fixed-size array of function pointers (no `std::vector` on MCU).
This decouples the sensor module from lister types.
Keep callbacks short to avoid blocking the publisher.
In C++, use `std::function` on capable targets or raw function pointers on bare-metal.

```cpp
template<typename T, int MaxSubs = 8>
class Subject {
    using Callback = void(*)(const T&);
    Callback subs_[MaxSubs]{};
    int count_ = 0;
public:
    void subscribe(Callback cb) {
        if (count_ < MaxSubs) subs_[count_++] = cb;
    }
    void notify(const T& data) {
        for (int i = 0; i < count_; i++) subs_[i](data);
    }
};

Subject<float> temperature_subject;
// Subscribers
void log_temp(const float& t) { logger.write(t); }
void check_interlock(const float& t) { if (t > 500) fault(); }
```

### 15. Message Queue

A thread-safe FIFO buffer between producer (ISR or task) and consumer (main loop or another task). Fixed-size, pre-allocated, no heap. The producer writes to head, consumer reads from tail. For ISR→main communication, use a lock-free single-producer single-consumer (SPSC) ring buffer with atomic indices. For multi-producer scenarios, disable interrupts briefly around the push or use compare-and-swap. Message queues decouple timing — the producer doesn't wait for the consumer, enabling asynchronous architectures.

```cpp
template<typename T, int N>
class MessageQueue {
    T buf_[N];
    std::atomic<int> head_{0}, tail_{0};
public:
    bool push(const T& item) {
        int h = head_.load(std::memory_order_relaxed);
        int next = (h + 1) % N;
        if (next == tail_.load(std::memory_order_acquire)) return false; // full
        buf_[h] = item;
        head_.store(next, std::memory_order_release);
        return true;
    }
    bool pop(T& item) {
        int t = tail_.load(std::memory_order_relaxed);
        if (t == head_.load(std::memory_order_acquire)) return false; // empty
        item = buf_[t];
        tail_.store((t + 1) % N, std::memory_order_release);
        return true;
    }
};
```

### 16. Command Pattern

Encapsulate each request (set temperature, open valve, start recipe) as a struct with an opcode and payload. Commands are queued, logged, and executed by a dispatcher. This enables undo/redo, command replay for diagnostics, and remote control — send the same command struct over UART or TCP. The executor switch-cases on the opcode and calls the appropriate handler. On larger systems, commands can be polymorphic objects. Fixed-size command structs ensure predictable memory usage.

```cpp
enum class CmdType : uint8_t { SET_TEMP, SET_PRESSURE, START_RECIPE, STOP };

struct Command {
    CmdType type;
    float value;
    uint32_t timestamp;
};

MessageQueue<Command, 16> cmd_queue;

void execute(const Command& cmd) {
    switch (cmd.type) {
        case CmdType::SET_TEMP:     target_temp = cmd.value;     break;
        case CmdType::SET_PRESSURE: target_pressure = cmd.value; break;
        case CmdType::START_RECIPE: state = State::PUMP_DOWN;    break;
        case CmdType::STOP:         state = State::IDLE;         break;
    }
    log_command(cmd);  // audit trail
}
```

### 17. Protocol Handler

Define a fixed binary struct for wire communication. Both endpoints share the same struct definition (or equivalent) to serialize/deserialize data. Include a header with magic bytes, sequence number, length, and CRC for framing and integrity. The handler implements a state machine: WAIT_HEADER → READ_PAYLOAD → VALIDATE_CRC → DISPATCH. Use `__attribute__((packed))` or `#pragma pack` to prevent compiler padding. Your `SimPacket` is a minimal version — adding framing and CRC makes it production-ready.

```cpp
#pragma pack(push, 1)
struct Frame {
    uint16_t magic;      // 0xABCD
    uint8_t  msg_type;
    uint16_t length;
    uint8_t  payload[128];
    uint32_t crc;
};
#pragma pack(pop)

enum class ParseState { SYNC, HEADER, PAYLOAD, CRC };

ParseState parse_byte(uint8_t byte) {
    static Frame frame;
    static int pos = 0;
    // Feed bytes into frame, transition states
    // On CRC valid → dispatch(frame)
    // On CRC fail → discard, resync on magic
}
```

### 18. Double Buffer

Maintain two buffers: one being written by the producer, one being read by the consumer.
When the producer finishes a frame, swap the pointers atomically.
The consumer always reads a complete, consistent frame — never a half-written one.
Zero-copy, no mutex needed if the swap is atomic.
Used for display framebuffers, sensor data frames, and network packet assembly.
On MCUs, use a `volatile` pointer swap.
On multi-core, use `std::atomic<T*>::exchange`.
Costs 2× memory but eliminates all read/write contention.

```cpp
template<typename T>
class DoubleBuffer {
    T buf_[2];
    std::atomic<int> write_idx_{0};
    std::atomic<int> read_idx_{1};
public:
    T& write_buf() { return buf_[write_idx_.load()]; }
    const T& read_buf() { return buf_[read_idx_.load()]; }
    void swap() {
        int w = write_idx_.load();
        int r = read_idx_.load();
        write_idx_.store(r);
        read_idx_.store(w);
    }
};

DoubleBuffer<SensorFrame> sensor_buf;
// ISR fills sensor_buf.write_buf(), calls swap()
// Main loop reads sensor_buf.read_buf() — always consistent
```

### 19. Ring Buffer / Circular Queue

A fixed-size array with head and tail indices that wrap around. 
`push` writes at head and advances; `pop` reads at tail and advances.
When head catches tail, the buffer is full.
No dynamic allocation, O(1) operations, cache-friendly.
The fundamental data structure in embedded systems — used for UART RX/TX buffers, audio streams, log histories, and inter-task communication.
Power-of-two sizes allow modulo via bitmask (`& (N-1)`) instead of division. Single-producer single-consumer variants need no locks.

```cpp
template<typename T, int N>
class RingBuffer {
    static_assert((N & (N-1)) == 0, "N must be power of 2");
    T buf_[N];
    int head_ = 0, tail_ = 0;
public:
    bool push(const T& v) {
        int next = (head_ + 1) & (N - 1);
        if (next == tail_) return false;
        buf_[head_] = v;
        head_ = next;
        return true;
    }
    bool pop(T& v) {
        if (head_ == tail_) return false;
        v = buf_[tail_];
        tail_ = (tail_ + 1) & (N - 1);
        return true;
    }
    int size() const { return (head_ - tail_) & (N - 1); }
};
```

---

## Safety / Reliability Patterns

### 20. Watchdog

A countdown timer that resets the system if not periodically "kicked." The main loop calls `watchdog_kick()` each cycle. If the software hangs (infinite loop, deadlock, unhandled exception), the timer expires and triggers a hardware reset. On MCUs, this is a hardware peripheral (IWDG on STM32). On macOS simulation, use a background thread with a timeout. Place the kick at the end of the main loop — only a fully completed cycle resets the timer. Never kick inside an ISR, as that defeats the purpose.

```cpp
class Watchdog {
    std::atomic<uint32_t> last_kick_;
    uint32_t timeout_ms_;
    std::thread monitor_;
public:
    Watchdog(uint32_t timeout_ms) : timeout_ms_(timeout_ms) {
        last_kick_ = hal::millis();
        monitor_ = std::thread([this] {
            while (true) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (hal::millis() - last_kick_.load() > timeout_ms_) {
                    std::cerr << "WATCHDOG TIMEOUT — resetting\n";
                    std::abort();
                }
            }
        });
    }
    void kick() { last_kick_ = hal::millis(); }
};
```

### 21. Heartbeat

Periodically send a "I'm alive" message to a supervisor (remote server, adjacent MCU, or monitoring task). If the supervisor misses N consecutive heartbeats, it declares the sender dead and takes recovery action (restart, failover, alarm). Unlike a watchdog (self-reset), a heartbeat enables external monitoring. Include diagnostic data in the heartbeat: uptime, cycle count, error counters, state. Your HMI dashboard could display a "last seen" timestamp for the controller — if stale, the operator knows something is wrong.

```cpp
struct Heartbeat {
    uint32_t uptime_sec;
    uint32_t cycle_count;
    uint8_t  state;
    uint8_t  error_count;
    uint16_t cpu_usage_pct;
};

void heartbeat_task() {
    static uint32_t last_send = 0;
    if (hal::millis() - last_send >= 1000) {
        Heartbeat hb{
            .uptime_sec = hal::millis() / 1000,
            .cycle_count = seq,
            .state = static_cast<uint8_t>(current_state),
            .error_count = fault_counter,
            .cpu_usage_pct = measure_cpu()
        };
        udp_send(SUPERVISOR_ADDR, &hb, sizeof(hb));
        last_send = hal::millis();
    }
}
```

### 22. Interlock

A safety condition that forces the system into a safe state when violated, regardless of what the control logic is doing. Interlocks are checked every cycle before actuator outputs are applied. They cannot be overridden by software — only by explicit operator acknowledgment after the fault is cleared. Implement as a chain of boolean checks; if any fails, all outputs go to their safe values. Your CVD controller's over-temperature check (`T > 500 → FAULT`) is an interlock. Production systems have hardware interlocks too (relay-based, independent of software).

```cpp
struct Interlock {
    const char* name;
    bool (*check)(const SensorData&);
    bool tripped = false;
};

Interlock interlocks[] = {
    {"OVER_TEMP",     [](const SensorData& s) { return s.temp > 500.0; }},
    {"OVER_PRESSURE", [](const SensorData& s) { return s.pressure > 800.0; }},
    {"RF_ARC",        [](const SensorData& s) { return s.rf_reflected > s.rf_forward * 0.5; }},
};

bool check_interlocks(const SensorData& s) {
    for (auto& il : interlocks) {
        if (il.check(s)) {
            il.tripped = true;
            std::cerr << "INTERLOCK: " << il.name << "\n";
            enter_safe_state();
            return false;
        }
    }
    return true;  // all clear
}
```

### 23. Redundancy (Triple Modular)

Read the same measurement from three independent sensors. Compare all three; take the majority vote. If one sensor disagrees, flag it as faulty and continue on the two agreeing sensors. If two disagree, enter a safe degraded mode. This tolerates single-sensor failures without false trips. In software simulation, model three "sensor" threads that add different noise/bias. TMR is used in aerospace, nuclear, and safety-critical automotive (ASIL-D). The voting logic must itself be simple and verifiable — typically under 20 lines.

```cpp
struct TmrSensor {
    float read_a, read_b, read_c;

    float vote() const {
        float ab = std::abs(read_a - read_b);
        float ac = std::abs(read_a - read_c);
        float bc = std::abs(read_b - read_c);
        constexpr float TOL = 5.0;

        if (ab < TOL && ac < TOL && bc < TOL)
            return (read_a + read_b + read_c) / 3.0f;  // all agree
        if (ab < TOL) return (read_a + read_b) / 2.0f;  // C is outlier
        if (ac < TOL) return (read_a + read_c) / 2.0f;  // B is outlier
        if (bc < TOL) return (read_b + read_c) / 2.0f;  // A is outlier
        return NAN;  // no consensus — enter safe mode
    }
};
```

### 24. Assertion / Contract

Use `assert()` or custom `REQUIRE()` macros to verify preconditions, postconditions, and invariants at runtime. In embedded C++, a failed assertion should log the file/line, dump diagnostics to flash, and enter a safe state — not call `abort()` blindly. Disable assertions in release builds only if performance requires it; otherwise, keep them on (a caught bug beats an undetected one). Design-by-contract style: each function documents what it expects and what it guarantees, enforced in code, not just comments.

```cpp
#define REQUIRE(cond, msg) \
    do { if (!(cond)) { \
        fault_log(__FILE__, __LINE__, msg); \
        enter_safe_state(); \
    }} while(0)

#define ENSURE(cond, msg) REQUIRE(cond, msg)  // postcondition

void set_heater_power(float pct) {
    REQUIRE(pct >= 0.0f && pct <= 100.0f, "heater power out of range");
    dac_write(HEATER_CH, pct / 100.0f * DAC_MAX);
    float readback = adc_read(HEATER_FB_CH);
    ENSURE(std::abs(readback - pct) < 5.0f, "heater feedback mismatch");
}
```

### 25. Safe State Pattern

Define a single function that forces all actuators to their safest configuration: heaters off, valves closed (or open, depending on fail-safe design), RF off, motors stopped. This function is called from every fault path — interlocks, watchdog, assertion failures, communication loss. It must be simple, have no dependencies on system state, and never fail. Test it explicitly. Your CVD FAULT state is a safe state: `heater_power=0, throttle=100 (open), rf=0, mfc=0`. The safe state should be reachable from any other state.

```cpp
void enter_safe_state() {
    // These must NEVER fail — direct register writes, no abstractions
    heater_power = 0;
    throttle_pos = 100.0;  // fail-open
    rf_setpoint = 0;
    std::memset(mfc_setpoints, 0, sizeof(mfc_setpoints));
    state = State::FAULT;
    log("SAFE STATE ENTERED");

    // Optionally: dump diagnostics to persistent storage
    flash_write_fault_record(last_sensor_data, hal::millis());
}
// Rule: enter_safe_state() must work even if heap is corrupted,
// stack is almost full, or ISRs are disabled.
```

### 26. CRC / Checksum

Append a checksum to every transmitted packet. The receiver computes the same checksum over the received data and compares. Mismatches indicate bit errors from noise, EMI, or buffer overruns. CRC-32 is standard for Ethernet/protocols; CRC-16 for lighter use; simple XOR checksums for minimal overhead. For your `SimPacket`, adding a 4-byte CRC at the end catches corruption on the TCP link (TCP has its own checksum, but end-to-end CRC catches application-layer bugs too). Use a lookup table for speed on MCUs.

```cpp
uint32_t crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

// Sender
void send_packet(int sock, SimPacket& pkt) {
    pkt.crc = crc32(reinterpret_cast<uint8_t*>(&pkt), offsetof(SimPacket, crc));
    send(sock, &pkt, sizeof(pkt), 0);
}

// Receiver
bool recv_packet(int sock, SimPacket& pkt) {
    recv(sock, &pkt, sizeof(pkt), MSG_WAITALL);
    uint32_t expected = crc32(reinterpret_cast<uint8_t*>(&pkt), offsetof(SimPacket, crc));
    return pkt.crc == expected;
}
```

---

## Resource Management Patterns

### 27. Static Allocation

Allocate all memory at compile time — global arrays, stack variables, `static` locals.
Zero use of `malloc`, `new`, or any dynamic allocator. This eliminates heap fragmentation, allocation failures, and memory leaks — the top three reliability killers in long-running embedded systems. Size every buffer for the worst case and `static_assert` that it fits in RAM. Your CVD controller uses this pattern — `SimPacket pkt{}` on the stack, fixed `gas_flow_recipe[8]` array. Most safety-critical standards (MISRA, DO-178C) require or strongly recommend static allocation only.

```cpp
// All memory determined at compile time
static SensorData sensor_history[256];    // ring buffer, pre-allocated
static char log_buffer[1024];             // formatted log output
static Command cmd_queue_storage[16];     // backing for message queue
static SimPacket tx_pkt, rx_pkt;          // reused each cycle

// Compile-time verification
static_assert(sizeof(sensor_history) + sizeof(log_buffer) +
              sizeof(cmd_queue_storage) < 32768,
              "total static allocation exceeds 32KB RAM budget");
```

### 28. Memory Pool

Pre-allocate a fixed-size array of blocks. A free-list (linked list through the blocks themselves) tracks available blocks. `alloc()` pops from the free list; `free()` pushes back. O(1) allocation, zero fragmentation, deterministic timing — unlike `malloc`. Used when you need variable-lifetime objects (network packets, event objects) but can't afford heap unpredictability. Each pool serves one object type/size. If the pool is exhausted, you get an immediate, handleable error rather than a fragmented heap that fails hours later.

```cpp
template<typename T, int N>
class MemoryPool {
    union Block {
        T data;
        Block* next;
    };
    Block storage_[N];
    Block* free_list_;
public:
    MemoryPool() {
        free_list_ = &storage_[0];
        for (int i = 0; i < N - 1; i++)
            storage_[i].next = &storage_[i + 1];
        storage_[N - 1].next = nullptr;
    }
    T* alloc() {
        if (!free_list_) return nullptr;
        Block* b = free_list_;
        free_list_ = b->next;
        return &b->data;
    }
    void free(T* ptr) {
        auto* b = reinterpret_cast<Block*>(ptr);
        b->next = free_list_;
        free_list_ = b;
    }
};
```

### 29. RAII (Resource Acquisition Is Initialization)

Acquire a resource (lock, file, peripheral enable) in a constructor; release it in the destructor. The compiler guarantees cleanup when the object leaves scope — even on exceptions or early returns. In embedded C++, use RAII for mutex locks (`LockGuard`), interrupt disable/enable pairs, peripheral power-on/off, and DMA buffer ownership. This eliminates "forgot to unlock" bugs. On bare-metal without exceptions, RAII still works via normal scope exit. The pattern costs zero runtime — constructors/destructors inline to the same instructions you'd write manually.

```cpp
class InterruptGuard {
public:
    InterruptGuard()  { __disable_irq(); }  // ARM intrinsic
    ~InterruptGuard() { __enable_irq(); }
    InterruptGuard(const InterruptGuard&) = delete;
    InterruptGuard& operator=(const InterruptGuard&) = delete;
};

void critical_operation() {
    InterruptGuard guard;  // interrupts disabled
    shared_counter++;
    shared_buffer[idx] = data;
}  // interrupts re-enabled automatically, even if function returns early

class SpiTransaction {
    SpiDevice& dev_;
public:
    SpiTransaction(SpiDevice& d) : dev_(d) { dev_.select(); }
    ~SpiTransaction() { dev_.deselect(); }
};
```

### 30. Singleton (Hardware)

A hardware peripheral (UART, SPI bus, ADC) has exactly one physical instance. The singleton pattern ensures only one software object manages it, preventing conflicting configurations. In embedded C++, use a function-local static (`Meyer's singleton`) — no heap, lazy-initialized, thread-safe since C++11. Provide access via `Uart::instance()`. Avoid the anti-pattern of making everything a singleton — only true hardware singletons deserve it. For testability, combine with the HAL pattern: the singleton returns an interface reference.

```cpp
class SystemUart {
    SystemUart() { /* configure UART registers */ }
public:
    SystemUart(const SystemUart&) = delete;
    SystemUart& operator=(const SystemUart&) = delete;

    static SystemUart& instance() {
        static SystemUart uart;  // created once, no heap
        return uart;
    }

    void send(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; i++) {
            while (!tx_ready()) {}
            write_reg(data[i]);
        }
    }
};

// Usage anywhere in codebase:
SystemUart::instance().send(buf, sizeof(buf));
```

---

## Control Patterns

### 31. PID Controller

The proportional-integral-derivative controller computes an output from the error between setpoint and measurement. P reacts to current error, I accumulates past error (eliminates steady-state offset), D predicts future error (dampens oscillation). Tune gains empirically: start with P only, add I to eliminate offset, add D to reduce overshoot. Clamp the integral term to prevent windup when the actuator saturates. Your CVD controller uses PID for temperature and pressure — the most common control algorithm in industrial embedded systems.

#### PID Math

Full continuous-time equation:

$$output = K_p \cdot e(t) + K_i \cdot \int_0^t e(\tau) \, d\tau + K_d \cdot \frac{de(t)}{dt}$$

where $e(t) = setpoint - measured$.

Discrete-time (per-cycle) breakdown:

| Term | Formula | Role | Alone fails because |
|------|---------|------|---------------------|
| **P** | $K_p \cdot error$ | React to current error | Steady-state offset, oscillation |
| **I** | $K_i \cdot \sum error \cdot dt$ | Eliminate accumulated past error | Slow, overshoots (windup) |
| **D** | $K_d \cdot \frac{error - prev\_error}{dt}$ | Dampen rate of change | Amplifies sensor noise |

Step-by-step per cycle:
1. Compute error: `error = setpoint - measured`
2. P term: `P = kp * error`
3. I term: `integral += error * dt`, then `I = ki * integral`
   - Anti-windup: clamp integral so it cannot grow unbounded when actuator is saturated
4. D term: `derivative = (error - prev_error) / dt`, then `D = kd * derivative`
5. Sum: `output = P + I + D`
6. Clamp output to actuator range `[out_min, out_max]`
7. Store `prev_error = error` for next cycle

Tuning order (Ziegler-Nichols):
1. Set Ki = 0, Kd = 0, increase Kp until system oscillates steadily
2. Add Ki to eliminate steady-state offset
3. Add Kd to dampen overshoot

CVD example (temperature → heater):
- setpoint = 400°C, measured = 385°C → error = 15
- P pushes heater power up proportionally
- I accumulates because temperature has been below target
- D sees temperature rising fast, eases off to prevent overshoot

```cpp
struct PID {
    double kp, ki, kd;
    double integral = 0, prev_error = 0;
    double out_min, out_max;

    double compute(double setpoint, double measured, double dt) {
        double error = setpoint - measured;
        integral += error * dt;
        // Anti-windup: clamp integral
        integral = std::clamp(integral, out_min / ki, out_max / ki);
        double derivative = (dt > 0) ? (error - prev_error) / dt : 0;
        prev_error = error;
        double output = kp * error + ki * integral + kd * derivative;
        return std::clamp(output, out_min, out_max);
    }
};
```

### 32. Debounce

Mechanical switches bounce for 5-50ms when pressed, generating multiple false edges. Debouncing requires the input to be stable for N consecutive reads before accepting a state change. Implement as a counter: each cycle, if the raw reading matches the pending state, increment; if it reaches the threshold, accept the transition. If it doesn't match, reset the counter. This filters electrical noise on any digital input — buttons, limit switches, proximity sensors. Timer-based debounce (ignore further edges for T ms after first edge) is simpler but less robust.

```cpp
class Debouncer {
    bool stable_state_ = false;
    bool pending_state_ = false;
    int count_ = 0;
    int threshold_;
public:
    Debouncer(int threshold = 5) : threshold_(threshold) {}

    bool update(bool raw_input) {
        if (raw_input == pending_state_) {
            count_++;
            if (count_ >= threshold_) {
                stable_state_ = pending_state_;
                count_ = threshold_;  // clamp
            }
        } else {
            pending_state_ = raw_input;
            count_ = 0;
        }
        return stable_state_;
    }
    bool state() const { return stable_state_; }
};
```

### 33. Hysteresis

Use different thresholds for on→off and off→on transitions to prevent rapid oscillation near a single threshold. Example: turn cooling fan on at 80°C, off at 70°C. Without hysteresis, a sensor reading fluctuating around 75°C would toggle the fan every cycle. The dead band (10°C in this example) must be wider than the sensor noise. Apply to any threshold-based decision: pressure relief valves, level sensors, thermostat control. In your CVD controller, adding hysteresis to the PUMP_DOWN→GAS_STABILIZE transition prevents oscillation near the 0.01 Torr boundary.

```cpp
class Hysteresis {
    double high_threshold_;   // turn ON above this
    double low_threshold_;    // turn OFF below this
    bool state_ = false;
public:
    Hysteresis(double low, double high)
        : high_threshold_(high), low_threshold_(low) {}

    bool update(double value) {
        if (state_ && value < low_threshold_)
            state_ = false;
        else if (!state_ && value > high_threshold_)
            state_ = true;
        return state_;
    }
};

// Usage: fan control
Hysteresis fan_control(70.0, 80.0);
bool fan_on = fan_control.update(current_temp);
```

### 34. Ramp / Slew Rate Limiter

Limit how fast an output can change per cycle to prevent mechanical shock, electrical spikes, or process upsets. Each cycle, clamp the output delta to `±max_rate * dt`. The output smoothly ramps toward the target instead of jumping. Used for motor speed commands, heater power ramps, valve position changes, and RF power ramp-up. In CVD processing, ramping RF power prevents plasma arcing; ramping temperature prevents thermal shock on wafers. Simple to implement, high safety value.

```cpp
class SlewLimiter {
    double value_;
    double max_rate_;  // max change per second
public:
    SlewLimiter(double initial, double max_rate)
        : value_(initial), max_rate_(max_rate) {}

    double update(double target, double dt) {
        double delta = target - value_;
        double max_delta = max_rate_ * dt;
        if (delta > max_delta) delta = max_delta;
        if (delta < -max_delta) delta = -max_delta;
        value_ += delta;
        return value_;
    }
    double value() const { return value_; }
};

// Usage: ramp heater from 0% to 100% at max 10%/sec
SlewLimiter heater_ramp(0.0, 10.0);
double cmd = heater_ramp.update(100.0, 0.1);  // 0→1→2→...→100
```

### 35. Moving Average / Filter

Maintain a ring buffer of the last N sensor readings. The output is the arithmetic mean, removing high-frequency noise while preserving the underlying trend. An exponential moving average (EMA) uses less memory — one variable instead of N — with a tunable smoothing factor α. Low α = more smoothing = more lag. Choose N or α based on noise frequency vs. response time requirements. For embedded, prefer integer arithmetic with fixed-point scaling when floating-point hardware is absent.

```cpp
// Ring buffer moving average
template<int N>
class MovingAverage {
    double buf_[N]{};
    int idx_ = 0;
    double sum_ = 0;
public:
    double update(double sample) {
        sum_ -= buf_[idx_];
        buf_[idx_] = sample;
        sum_ += sample;
        idx_ = (idx_ + 1) % N;
        return sum_ / N;
    }
};

// Exponential moving average (1 variable, no buffer)
class EmaFilter {
    double value_ = 0;
    double alpha_;
public:
    EmaFilter(double alpha) : alpha_(alpha) {}
    double update(double sample) {
        value_ = alpha_ * sample + (1.0 - alpha_) * value_;
        return value_;
    }
};
```

### 36. Calibration Table

Map raw sensor values (ADC counts, resistance, voltage) to physical units (°C, Torr, sccm) using a lookup table with linear interpolation. Store the table in flash/const memory. During manufacturing, characterize each unit and write its calibration table. At runtime, binary-search the table for the two nearest entries and interpolate. This handles nonlinear sensors (thermocouples, thermistors, pressure transducers) without computing complex polynomials. Tables are easy to update in the field without recompiling firmware.

```cpp
struct CalPoint { double raw; double physical; };

constexpr CalPoint thermo_cal[] = {
    {0,    -50.0},
    {1000, 0.0},
    {2048, 25.0},
    {3000, 100.0},
    {3500, 200.0},
    {3900, 400.0},
    {4095, 500.0},
};
constexpr int CAL_SIZE = sizeof(thermo_cal) / sizeof(CalPoint);

double calibrate(double raw) {
    if (raw <= thermo_cal[0].raw) return thermo_cal[0].physical;
    for (int i = 1; i < CAL_SIZE; i++) {
        if (raw <= thermo_cal[i].raw) {
            double frac = (raw - thermo_cal[i-1].raw) /
                          (thermo_cal[i].raw - thermo_cal[i-1].raw);
            return thermo_cal[i-1].physical +
                   frac * (thermo_cal[i].physical - thermo_cal[i-1].physical);
        }
    }
    return thermo_cal[CAL_SIZE-1].physical;
}
```

---

## Architecture Patterns

### 37. Layered Architecture

Organize code into strict layers: Application → Services → Drivers → HAL. Each layer only calls the layer directly below it — never up or sideways. This enforces separation of concerns and makes each layer independently testable. The Application layer contains state machines and recipes. Services provide reusable logic (PID, filtering, logging). Drivers manage peripheral protocols (SPI transactions, UART framing). HAL wraps raw register access. Map this to directories: `app/`, `services/`, `drivers/`, `hal/`. Build rules enforce dependency direction.

```
Directory structure:
  src/
  ├── app/          ← state machines, recipes (calls services only)
  │   └── recipe_controller.cpp
  ├── services/     ← PID, filters, protocol (calls drivers only)
  │   ├── pid.cpp
  │   └── packet_handler.cpp
  ├── drivers/      ← SPI, UART, ADC framing (calls HAL only)
  │   ├── spi_driver.cpp
  │   └── uart_driver.cpp
  └── hal/          ← register access, GPIO (calls nothing)
      ├── hal_stm32.cpp
      └── hal_sim.cpp   ← macOS simulation
```

```cpp
// CMake enforces layers:
// app links services; services links drivers; drivers links hal
// app cannot directly #include anything from hal/
```

### 38. Component-Based

Each subsystem (heater, vacuum, gas, RF) is a self-contained component with a uniform interface: `init()`, `update(dt)`, `get_status()`, `shutdown()`. Components are registered in a list; the main loop iterates and updates all of them. Components communicate via message passing or shared state, not direct calls. This makes it easy to add/remove subsystems, test each in isolation, and reuse across projects. Your CVD system has natural components: heater-control, pressure-control, gas-delivery, RF-power, and safety-monitor.

```cpp
class IComponent {
public:
    virtual ~IComponent() = default;
    virtual void init() = 0;
    virtual void update(double dt) = 0;
    virtual const char* name() = 0;
};

class HeaterControl : public IComponent {
    PID pid_{2.0, 0.1, 0.5, 0, 0, 0.0, 100.0};
public:
    void init() override { /* configure DAC channel */ }
    void update(double dt) override {
        double cmd = pid_.compute(target_temp, read_temp(), dt);
        set_heater(cmd);
    }
    const char* name() override { return "Heater"; }
};

// Main loop
IComponent* components[] = {&heater, &vacuum, &gas, &rf, &safety};
for (auto* c : components) c->update(dt);
```

### 39. Model-View-Controller (MVC)

Separate the system into Model (physical state + simulation), View (HMI display), and Controller (decision logic + actuator commands). The Model updates independently (physics simulation). The Controller reads Model state and writes commands. The View reads Model state for display — it never modifies it. Changes to the HMI don't affect control logic. Your existing architecture already follows MVC: `simulator.py` = Model, `main.cpp` = Controller, `index.html` = View. The TCP protocol is the interface boundary between them.

```cpp
// Model — owns state, no I/O
struct ChamberModel {
    double temperature, pressure, rf_power;
    void step(double heater, double throttle, double rf, double dt) {
        // physics update
    }
};

// Controller — reads model, computes commands
struct Controller {
    PID temp_pid, pres_pid;
    void update(const ChamberModel& model, ActuatorCmd& cmd, double dt) {
        cmd.heater = temp_pid.compute(400.0, model.temperature, dt);
        cmd.throttle = pres_pid.compute(2.0, model.pressure, dt);
    }
};

// View — reads model, never writes
struct HmiView {
    void render(const ChamberModel& model) {
        display.print("T=%.1f P=%.2f", model.temperature, model.pressure);
    }
};
```

### 40. Reactor Pattern

Event-driven I/O multiplexing: a single thread monitors multiple I/O sources (sockets, serial ports, timers) using `select()`, `poll()`, or `epoll()`. When an event fires, the reactor dispatches to the registered handler. No threads, no blocking reads — efficient and deterministic. Used in network servers and embedded gateways that handle multiple connections. On macOS, use `kqueue`. The pattern scales from bare-metal (poll GPIO + UART + timer flags in super loop) to Linux-based edge devices (poll dozens of sockets). Combines naturally with the super loop pattern.

```cpp
#include <poll.h>
#include <functional>

struct Reactor {
    struct Source {
        int fd;
        std::function<void()> handler;
    };
    std::vector<Source> sources;

    void add(int fd, std::function<void()> handler) {
        sources.push_back({fd, handler});
    }

    void run() {
        std::vector<pollfd> pfds;
        for (auto& s : sources)
            pfds.push_back({s.fd, POLLIN, 0});

        while (true) {
            poll(pfds.data(), pfds.size(), 100 /* ms timeout */);
            for (size_t i = 0; i < pfds.size(); i++)
                if (pfds[i].revents & POLLIN)
                    sources[i].handler();
        }
    }
};

// Usage
Reactor reactor;
reactor.add(sim_socket, [&]{ handle_simulator_packet(); });
reactor.add(hmi_socket, [&]{ handle_hmi_command(); });
reactor.add(timer_fd,   [&]{ watchdog_kick(); });
reactor.run();
```
