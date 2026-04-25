# Semiconductor Equipment Controller Tech Stacks

Three dominant architectural patterns exist for semiconductor equipment (CVD, etch, litho, metrology, etc.) control systems. Each reflects a different philosophy on how to combine real-time control, safety, and fab integration.

---

## 1. PLC-Centric Stack

The controller is a **programmable logic controller** (PLC) runtime. Core control logic is written in IEC 61131-3 languages (Structured Text, Ladder, Function Block Diagram). C++ modules are added for complex algorithms.

```
┌────────────────────────────────────────────────────────────────┐
│ HMI (Operator Interface)                                       │
│ C# / WPF / .NET on Windows  — or —  Web-based (HTML5)        │
│ Communicates via ADS (TwinCAT) or OPC UA                       │
└────────────────────────┬───────────────────────────────────────┘
                         │ ADS / OPC UA
┌────────────────────────┴───────────────────────────────────────┐
│ PLC Runtime (real-time)                                        │
│                                                                 │
│  Platform : Beckhoff TwinCAT 3 (x86 + Windows)                │
│             or Siemens S7 / TIA Portal                         │
│             or Rockwell / Allen-Bradley                        │
│                                                                 │
│  Languages: Structured Text (IEC 61131-3)                      │
│             + C++ modules (TcCOM in TwinCAT)                   │
│                                                                 │
│  Functions: recipe sequencer, state machine, PID loops,        │
│             safety interlocks, data logging                    │
│                                                                 │
│  Cycle time: 250 μs – 10 ms (deterministic)                   │
└────────────────────────┬───────────────────────────────────────┘
                         │ EtherCAT (native for Beckhoff)
                         │ Profinet (native for Siemens)
                         │ EtherNet/IP (native for Rockwell)
┌────────────────────────┴───────────────────────────────────────┐
│ Field I/O                                                      │
│ Analog / digital terminals, MFCs, RF generators, valves        │
└────────────────────────────────────────────────────────────────┘
```

**Who uses this:**
- ASM International (TwinCAT + Beckhoff)
- ASML subsystems (TwinCAT + Beckhoff)
- Aixtron (TwinCAT + Beckhoff)
- Meyer Burger (Siemens S7)
- Many European and some Asian OEMs

**Characteristics:**
- Rapid development with PLC visual tools
- Strong vendor ecosystem (Beckhoff, Siemens sell complete hardware + software + I/O)
- C++ used only where PLC languages are insufficient (complex math, ML inference)
- Safety PLC (TwinSAFE, Siemens F-CPU) integrated into same platform
- EtherCAT/Profinet fieldbus tightly integrated

---

## 2. Embedded OS-Centric Stack

The controller runs a **real-time operating system** (RTOS) or RT-patched Linux. The application is a custom C/C++ program that owns the entire control loop.

```
┌────────────────────────────────────────────────────────────────┐
│ HMI (Operator Interface)                                       │
│ Qt on Linux / Windows  — or —  Web-based (React + WebSocket)  │
│ Separate PC or same box (non-RT process)                       │
└────────────────────────┬───────────────────────────────────────┘
                         │ TCP / shared memory / IPC
┌────────────────────────┴───────────────────────────────────────┐
│ RTOS / RT-Linux (real-time)                                    │
│                                                                 │
│  OS      : VxWorks / QNX / Linux + PREEMPT_RT / Xenomai       │
│  Language : C / C++17                                          │
│  Hardware : x86 industrial PC / ARM SBC / PowerPC (legacy)    │
│                                                                 │
│  Application:                                                   │
│    main() → spawns RT thread(s) → cyclic control loop          │
│    ├── read sensors (fieldbus driver)                          │
│    ├── check interlocks                                        │
│    ├── execute recipe step                                     │
│    ├── run PID controllers                                     │
│    ├── write actuators                                         │
│    └── log data                                                │
│                                                                 │
│  Cycle time: 1 – 10 ms (RTOS), 100 μs – 1 ms (Xenomai)       │
└────────────────────────┬───────────────────────────────────────┘
                         │ EtherCAT (via SOEM / IgH master)
                         │ Modbus RTU/TCP, DeviceNet, custom serial
┌────────────────────────┴───────────────────────────────────────┐
│ Field I/O                                                      │
│ EtherCAT slaves, analog boards, DAQ cards, serial devices      │
└────────────────────────────────────────────────────────────────┘
```

**Who uses this:**
- Applied Materials (VxWorks, some Linux)
- Lam Research (VxWorks)
- KLA (VxWorks + Linux)
- Tokyo Electron / TEL (QNX, Linux)
- Screen Semiconductor (Linux)
- Most American and Japanese OEMs

**Characteristics:**
- Full flexibility — you own every line of code
- Harder to develop (no PLC drag-and-drop, all C/C++)
- Better for complex algorithms (ML inference, computational lithography)
- Must implement safety logic yourself (or pair with a separate safety PLC)
- Fieldbus driver is a user-space or kernel module you integrate

---

## 3. Hybrid Stack

Combines a PLC for deterministic I/O control with an embedded Linux/Windows application for higher-level logic. Increasingly common as tools get more complex.

```
┌────────────────────────────────────────────────────────────────┐
│ HMI + Fab Host (SECS/GEM)                                      │
│ Windows or Linux application                                    │
└────────────────────────┬───────────────────────────────────────┘
                         │ OPC UA / TCP
┌────────────────────────┴───────────────────────────────────────┐
│ Equipment Controller (Linux / Windows)                          │
│ C++ / Python                                                    │
│ ├── Recipe management                                          │
│ ├── SECS/GEM host interface                                    │
│ ├── Data collection, ML inference                              │
│ └── Sends setpoints to PLC                                     │
└────────────────────────┬───────────────────────────────────────┘
                         │ OPC UA / ADS / Modbus TCP
┌────────────────────────┴───────────────────────────────────────┐
│ PLC (real-time)                                                │
│ Structured Text / Ladder                                        │
│ ├── Fast control loops (PID, interlock)                        │
│ ├── Safety logic                                               │
│ └── Direct I/O access                                          │
└────────────────────────┬───────────────────────────────────────┘
                         │ EtherCAT / Profinet
┌────────────────────────┴───────────────────────────────────────┐
│ Field I/O                                                      │
└────────────────────────────────────────────────────────────────┘
```

**Who uses this:**
- Newer tools from AMAT and Lam (Linux supervisor + PLC for I/O)
- ASML (TwinCAT subsystems + Linux supervisor)
- Emerging Chinese semi equipment vendors (e.g., AMEC, Naura)

---

## Comparison

| Aspect | PLC-Centric | Embedded OS-Centric | Hybrid |
|---|---|---|---|
| **Core language** | Structured Text + C++ modules | C / C++ | C++ (supervisor) + ST (PLC) |
| **RT guarantee** | Built-in (PLC vendor provides) | You configure (RTOS/kernel) | PLC handles RT; supervisor is non-RT |
| **Development speed** | Fast (visual PLC tools) | Slow (all custom code) | Medium |
| **Flexibility** | Limited by PLC paradigm | Unlimited | Best of both |
| **Safety** | Integrated safety PLC (SIL 2/3) | Separate safety system needed | PLC handles safety |
| **Cost** | High (Beckhoff/Siemens licenses) | Low (open-source OS/tools) | Medium |
| **ML/advanced algorithms** | Hard in PLC; need C++ module | Native | Supervisor handles ML |
| **Vendor lock-in** | High (Beckhoff, Siemens ecosystem) | Low | Medium |
| **Fab host (SECS/GEM)** | Separate Windows app usually | Same or separate process | Supervisor handles it |

---

## Supporting Technologies (common across all stacks)

### Fab Communication
| Standard | Purpose |
|---|---|
| SECS-II (SEMI E5) | Message format for host communication |
| HSMS (SEMI E37) | TCP transport for SECS-II |
| GEM (SEMI E30) | Equipment behavior model |
| EDA / Interface A (SEMI E120-E164) | High-speed data collection |
| OPC UA | Emerging alternative to SECS/GEM in some fabs |

### Fieldbus Protocols
| Protocol | Layer | Associated PLC ecosystem |
|---|---|---|
| EtherCAT | L2 raw Ethernet | Beckhoff (native), others via SOEM/IgH |
| Profinet | L2 (RT) / L3 (TCP) | Siemens |
| EtherNet/IP | L3 TCP/UDP | Rockwell / Allen-Bradley |
| Modbus RTU | Serial RS-485 | Universal (legacy devices) |
| Modbus TCP | L3 TCP | Universal |
| DeviceNet | CAN-based | Rockwell (legacy) |
| CC-Link IE | L2 | Mitsubishi (common in Japanese fabs) |

### Safety Standards
| Standard | Scope |
|---|---|
| SEMI S2 | Environmental, health, safety for semi equipment |
| SEMI S8 | Ergonomics |
| IEC 61508 / IEC 62061 | Functional safety (SIL levels) |
| ISO 13849 | Safety of machinery (Performance Levels) |

### Build & DevOps
| Tool | Role |
|---|---|
| CMake | C++ build system (universal) |
| Conan / vcpkg | C++ package manager |
| TwinCAT XAE (Visual Studio plugin) | PLC-centric build/debug |
| TIA Portal | Siemens PLC programming |
| Git | Version control |
| Jenkins / GitHub Actions / Azure DevOps | CI/CD |
| Google Test / Catch2 | C++ unit testing |
| PLCopen test frameworks | PLC unit testing |
