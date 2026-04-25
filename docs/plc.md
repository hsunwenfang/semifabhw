# PLC-Centric Stack for Semiconductor Equipment

A detailed reference for the Beckhoff TwinCAT-based PLC-centric architecture used by ASM, ASML subsystems, Aixtron, and others.

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        Equipment Software Stack                          │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │ Layer 4: Fab Host Interface                                     │    │
│  │ Windows Service / C# app                                        │    │
│  │ SECS/GEM (E5/E30/E37) over HSMS (TCP)                         │    │
│  │ Communicates recipe downloads, lot tracking, alarms to MES      │    │
│  └──────────────────────────┬──────────────────────────────────────┘    │
│                              │ ADS / OPC UA / TCP                       │
│  ┌──────────────────────────┴──────────────────────────────────────┐    │
│  │ Layer 3: HMI (Operator Interface)                               │    │
│  │ C# / WPF / .NET on Windows                                     │    │
│  │ ├── Live process display (temp, pressure, gas flow)             │    │
│  │ ├── Recipe editor                                               │    │
│  │ ├── Alarm viewer                                                │    │
│  │ └── Maintenance screens                                         │    │
│  │ Connects to PLC via ADS (Automation Device Specification)       │    │
│  └──────────────────────────┬──────────────────────────────────────┘    │
│                              │ ADS (TCP port 48898)                     │
│  ┌──────────────────────────┴──────────────────────────────────────┐    │
│  │ Layer 2: PLC Runtime (TwinCAT 3)          *** REAL-TIME ***    │    │
│  │ Runs on Beckhoff IPC (x86), replaces Windows scheduler          │    │
│  │                                                                  │    │
│  │  ┌─────────────────┐ ┌──────────────┐ ┌──────────────────────┐ │    │
│  │  │ PLC Project     │ │ C++ Module   │ │ Safety PLC           │ │    │
│  │  │ (Structured     │ │ (TcCOM)      │ │ (TwinSAFE)           │ │    │
│  │  │  Text)          │ │              │ │                      │ │    │
│  │  │ • State machine │ │ • Complex    │ │ • SIL 2/3 certified  │ │    │
│  │  │ • Recipe engine │ │   algorithms │ │ • E-stop             │ │    │
│  │  │ • PID loops     │ │ • ML infer.  │ │ • Gas leak shutoff   │ │    │
│  │  │ • Sequencing    │ │ • Custom     │ │ • Over-temp          │ │    │
│  │  │ • Interlocks    │ │   protocols  │ │ • Door interlock     │ │    │
│  │  └────────┬────────┘ └──────┬───────┘ └──────────┬───────────┘ │    │
│  │           └─────────────────┴────────────────────┘              │    │
│  │                              │ Internal process image           │    │
│  └──────────────────────────────┴──────────────────────────────────┘    │
│                                 │ EtherCAT (1 ms or faster cycle)      │
│  ┌──────────────────────────────┴──────────────────────────────────┐    │
│  │ Layer 1: Field I/O                                              │    │
│  │                                                                  │    │
│  │  EK1100 ─── EL3064 ─── EL4034 ─── EL2004 ─── EL1004           │    │
│  │  coupler    4ch AI     4ch AO     4ch DO     4ch DI             │    │
│  │             (thermo-   (MFC      (valves)   (limit              │    │
│  │              couples)   setpoints)            switches)          │    │
│  │                                                                  │    │
│  │  Third-party EtherCAT slaves:                                   │    │
│  │  ├── MKS MFC with EtherCAT interface                           │    │
│  │  ├── Advanced Energy RF generator (EtherCAT / Modbus gateway)  │    │
│  │  └── VAT throttle valve (EtherCAT)                              │    │
│  └─────────────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Hardware Bill of Materials (typical CVD chamber controller)

| Component | Model (example) | Role | ~Cost |
|---|---|---|---|
| Industrial PC | Beckhoff CX5140 (Intel Atom x5, 4-core) | Runs TwinCAT 3 + Windows | $1,500 |
| EtherCAT coupler | Beckhoff EK1100 | Bus coupler, first node in chain | $100 |
| Analog input (thermocouple) | Beckhoff EL3314 (4-ch TC) | Read susceptor, wall, showerhead temps | $200 |
| Analog input (voltage) | Beckhoff EL3064 (4-ch 0-10V) | Read pressure transducer (Baratron) | $150 |
| Analog output | Beckhoff EL4034 (4-ch 0-10V) | MFC flow setpoints, heater power setpoint | $200 |
| Digital output | Beckhoff EL2004 (4-ch 24V) | Pneumatic valves (gas on/off, vent, pump) | $80 |
| Digital input | Beckhoff EL1004 (4-ch 24V) | Limit switches, door sensor, EMO status | $60 |
| Safety terminals | Beckhoff EL6900 + EL1904 + EL2904 | TwinSAFE logic + safe I/O | $500 |

Total I/O hardware for one chamber: **~$3,000 – $5,000** (excluding the actual process hardware like MFCs, RF gen, etc.)

---

## Software Project Structure

```
cvd_tool/
├── TwinCAT/
│   ├── cvd_plc/                          # TwinCAT PLC project
│   │   ├── POUs/                         # Program Organization Units
│   │   │   ├── MAIN.TcPOU               # Entry point, calls subsystems
│   │   │   ├── FB_ChamberStateMachine.TcPOU
│   │   │   ├── FB_RecipeEngine.TcPOU
│   │   │   ├── FB_GasSystem.TcPOU
│   │   │   ├── FB_RFController.TcPOU
│   │   │   ├── FB_ThermalController.TcPOU
│   │   │   ├── FB_PressureController.TcPOU
│   │   │   └── FB_InterlockManager.TcPOU
│   │   ├── DUTs/                         # Data Unit Types (structs/enums)
│   │   │   ├── ST_SensorData.TcDUT
│   │   │   ├── ST_ActuatorCmd.TcDUT
│   │   │   ├── ST_RecipeStep.TcDUT
│   │   │   └── E_ChamberState.TcDUT
│   │   ├── GVLs/                         # Global Variable Lists
│   │   │   ├── GVL_IO.TcGVL             # mapped to EtherCAT I/O
│   │   │   ├── GVL_Process.TcGVL        # process variables
│   │   │   └── GVL_Alarms.TcGVL
│   │   └── PlcTask.TcTASK               # cycle time config (1 ms)
│   │
│   ├── cvd_safety/                       # TwinSAFE project
│   │   └── SafetyPLC.TcSPOU             # safety logic (certified SIL 2)
│   │
│   └── cvd_cpp_module/                   # TcCOM C++ module (optional)
│       ├── CvdAlgorithm.h
│       └── CvdAlgorithm.cpp             # e.g., model-based control, ML inference
│
├── HMI/
│   ├── CvdHmi.sln                        # C# / WPF solution
│   ├── Views/
│   │   ├── MainWindow.xaml
│   │   ├── ProcessView.xaml              # real-time gauges, charts
│   │   ├── RecipeEditor.xaml
│   │   └── AlarmView.xaml
│   ├── ViewModels/
│   │   └── ProcessViewModel.cs           # reads PLC vars via ADS
│   └── Services/
│       ├── AdsService.cs                 # TwinCAT ADS client
│       └── SecsGemService.cs             # SECS/GEM host communication
│
└── Docs/
    ├── IO_Mapping.xlsx                   # maps EtherCAT terminals to process signals
    └── StateChart.drawio                 # chamber state machine diagram
```

---

## Structured Text Examples

### Chamber State Machine

```iecst
FUNCTION_BLOCK FB_ChamberStateMachine
VAR_INPUT
    bStartProcess   : BOOL;
    bAbort          : BOOL;
    stSensors       : ST_SensorData;
    stRecipe        : ST_RecipeStep;
END_VAR
VAR_OUTPUT
    eState          : E_ChamberState;
    stActuators     : ST_ActuatorCmd;
END_VAR
VAR
    fbGas           : FB_GasSystem;
    fbRF            : FB_RFController;
    fbThermal       : FB_ThermalController;
    fbPressure      : FB_PressureController;
    fbInterlock     : FB_InterlockManager;
    tStepTimer      : TON;    (* step duration timer *)
END_VAR

(* Check interlocks FIRST — every cycle *)
fbInterlock(stSensors := stSensors);
IF fbInterlock.bTripped THEN
    eState := E_ChamberState.ABORT;
END_IF

CASE eState OF

E_ChamberState.IDLE:
    stActuators.bPumpOn := FALSE;
    stActuators.fHeaterPower := 0.0;
    IF bStartProcess THEN
        eState := E_ChamberState.PUMP_DOWN;
    END_IF

E_ChamberState.PUMP_DOWN:
    stActuators.bPumpOn := TRUE;
    stActuators.fThrottlePos := 100.0;    (* fully open to pump *)
    IF stSensors.fPressure < 0.01 THEN    (* base pressure reached *)
        eState := E_ChamberState.GAS_STABILIZE;
    END_IF

E_ChamberState.GAS_STABILIZE:
    fbGas(stRecipe := stRecipe, stSensors := stSensors);
    stActuators.fMfcSetpoints := fbGas.fSetpoints;
    fbPressure(fSetpoint := stRecipe.fPressure, fActual := stSensors.fPressure);
    stActuators.fThrottlePos := fbPressure.fOutput;
    IF fbGas.bStable AND fbPressure.bStable THEN
        eState := E_ChamberState.PROCESS;
        tStepTimer(IN := FALSE);    (* reset timer *)
    END_IF

E_ChamberState.PROCESS:
    fbGas(stRecipe := stRecipe, stSensors := stSensors);
    fbRF(fSetpoint := stRecipe.fRFPower, fReflected := stSensors.fRFReflected);
    fbThermal(fSetpoint := stRecipe.fTemperature, fActual := stSensors.fTemperature);
    fbPressure(fSetpoint := stRecipe.fPressure, fActual := stSensors.fPressure);

    stActuators.fMfcSetpoints := fbGas.fSetpoints;
    stActuators.fRFPower := fbRF.fOutput;
    stActuators.fHeaterPower := fbThermal.fOutput;
    stActuators.fThrottlePos := fbPressure.fOutput;

    tStepTimer(IN := TRUE, PT := stRecipe.tDuration);
    IF tStepTimer.Q THEN
        eState := E_ChamberState.PURGE;
    END_IF

E_ChamberState.PURGE:
    stActuators.fRFPower := 0.0;
    stActuators.fMfcSetpoints := stRecipe.fPurgeFlows;
    tStepTimer(IN := TRUE, PT := T#10S);
    IF tStepTimer.Q THEN
        eState := E_ChamberState.VENT;
    END_IF

E_ChamberState.VENT:
    stActuators.bPumpOn := FALSE;
    stActuators.bVentValve := TRUE;
    IF stSensors.fPressure > 700.0 THEN    (* near atmospheric *)
        stActuators.bVentValve := FALSE;
        eState := E_ChamberState.IDLE;
    END_IF

E_ChamberState.ABORT:
    (* Emergency: shut everything off *)
    stActuators.fRFPower := 0.0;
    stActuators.fHeaterPower := 0.0;
    stActuators.fMfcSetpoints := 0.0;    (* close all gas *)
    stActuators.bPumpOn := TRUE;          (* keep pumping for safety *)
    IF NOT fbInterlock.bTripped AND NOT bAbort THEN
        eState := E_ChamberState.IDLE;
    END_IF

END_CASE
```

### PID Controller (Thermal)

```iecst
FUNCTION_BLOCK FB_ThermalController
VAR_INPUT
    fSetpoint   : REAL;     (* °C *)
    fActual     : REAL;     (* °C from thermocouple *)
END_VAR
VAR_OUTPUT
    fOutput     : REAL;     (* 0-100% heater power *)
    bStable     : BOOL;
END_VAR
VAR
    fbPID       : FB_BasicPID;  (* Beckhoff PID library or custom *)
    fKp         : REAL := 2.0;
    fKi         : REAL := 0.1;
    fKd         : REAL := 0.5;
END_VAR

fbPID(
    fSetpointValue := fSetpoint,
    fActualValue   := fActual,
    fKp := fKp,
    fTn := 1.0 / fKi,    (* integral time *)
    fTv := fKd / fKp,     (* derivative time *)
    fOutMaxLimit := 100.0,
    fOutMinLimit := 0.0
);

fOutput := fbPID.fOut;
bStable := ABS(fSetpoint - fActual) < 2.0;    (* within 2°C *)
```

---

## Data Types

```iecst
TYPE E_ChamberState :
(
    IDLE        := 0,
    PUMP_DOWN   := 10,
    LEAK_CHECK  := 20,
    GAS_STABILIZE := 30,
    PROCESS     := 40,
    PURGE       := 50,
    VENT        := 60,
    ABORT       := 99
);
END_TYPE

TYPE ST_SensorData :
STRUCT
    fTemperature    : REAL;     (* °C — susceptor *)
    fPressure       : REAL;     (* Torr *)
    fRFForward      : REAL;     (* W *)
    fRFReflected    : REAL;     (* W *)
    afGasFlows      : ARRAY[1..8] OF REAL;  (* sccm per line *)
    bDoorClosed     : BOOL;
    bEMO            : BOOL;     (* emergency machine off *)
    bGasLeakDetect  : BOOL;
END_STRUCT
END_TYPE

TYPE ST_ActuatorCmd :
STRUCT
    fHeaterPower    : REAL;     (* 0-100% *)
    fRFPower        : REAL;     (* W setpoint *)
    fThrottlePos    : REAL;     (* 0-100% *)
    fMfcSetpoints   : ARRAY[1..8] OF REAL;  (* sccm per line *)
    bPumpOn         : BOOL;
    bVentValve      : BOOL;
END_STRUCT
END_TYPE

TYPE ST_RecipeStep :
STRUCT
    sName           : STRING(80);
    fTemperature    : REAL;
    fPressure       : REAL;
    fRFPower        : REAL;
    afGasFlows      : ARRAY[1..8] OF REAL;
    fPurgeFlows     : ARRAY[1..8] OF REAL;
    tDuration       : TIME;
END_STRUCT
END_TYPE
```

---

## ADS Communication (HMI ↔ PLC)

ADS (Automation Device Specification) is TwinCAT's native protocol. The HMI reads/writes PLC variables by name:

```csharp
// C# HMI — reading PLC variables via ADS
using TwinCAT.Ads;

var client = new AdsClient();
client.Connect(AmsNetId.Local, 851);    // PLC runtime port

// Read sensor data
var temp = (float)client.ReadValue("GVL_Process.stSensors.fTemperature", typeof(float));
var pressure = (float)client.ReadValue("GVL_Process.stSensors.fPressure", typeof(float));
var state = (int)client.ReadValue("GVL_Process.eChamberState", typeof(int));

// Write recipe command
client.WriteValue("GVL_Process.bStartProcess", true);

// Subscribe to changes (event-driven, not polling)
client.AddDeviceNotification(
    "GVL_Process.stSensors.fTemperature",
    new NotificationSettings(AdsTransMode.OnChange, 100, 0),  // 100ms max delay
    (sender, e) => {
        float newTemp = BitConverter.ToSingle(e.Data.ToArray(), 0);
        UpdateDisplay(newTemp);
    }
);
```

---

## Development Workflow

```
1. Design I/O mapping (Excel → import to TwinCAT)
2. Write Structured Text PLC code in TwinCAT XAE (Visual Studio plugin)
3. Write C++ TcCOM modules for complex algorithms (optional)
4. Write safety logic in TwinSAFE editor
5. Simulate:
   ├── TwinCAT has a built-in PLC simulator (runs on dev PC without hardware)
   ├── Use "simulation mode" to inject fake sensor values
   └── Test state machine transitions, PID tuning
6. Deploy to Beckhoff IPC:
   ├── TwinCAT XAE → "Activate Configuration" → downloads to target
   └── Or use TwinCAT Automation Interface (CLI/script deployment)
7. Commission on real hardware:
   ├── EtherCAT scan → auto-detect connected I/O terminals
   ├── Map I/O variables
   └── Tune PIDs with real process feedback
```

---

## TwinCAT vs. Embedded OS-Centric Comparison

| Aspect | TwinCAT (PLC-centric) | VxWorks/Linux (embedded OS) |
|---|---|---|
| Dev environment | Visual Studio (TwinCAT XAE plugin) | vim/VSCode + CMake + GDB |
| Debug | Online PLC debug, variable watch, force values | GDB, printf, JTAG |
| I/O integration | Scan EtherCAT → drag-drop → done | Write SOEM/IgH driver code, map manually |
| Simulation | Built-in PLC simulator | Must build your own mock layer |
| Safety | TwinSAFE (SIL 3 certified, integrated) | Separate safety PLC required |
| License cost | ~$500–2,000 per target (TwinCAT runtime license) | Free (Linux/GCC) or ~$10k+ (VxWorks) |
| Determinism | Guaranteed by Beckhoff kernel (sub-μs jitter) | Depends on your RTOS config |
| Learning curve | PLC engineers ramp fast; C++ devs feel constrained | C++ devs feel at home; PLC engineers struggle |
