# AKS-on-PLC: Simulating a Semi Equipment Stack on Azure

This document illustrates how to use Azure Kubernetes Service (AKS) with 1 ARM node + 1 x86 node to simulate a PLC-centric semiconductor equipment controller stack, and its limitations.

---

## Target: What We're Simulating

A real CVD tool has:

```
HMI (Windows) ──ADS──► Beckhoff IPC (TwinCAT PLC) ──EtherCAT──► I/O terminals ──► Chamber
                              │
                        SECS/GEM ──TCP──► Fab MES
```

We replicate this in AKS:

```
HMI (React) ──WebSocket──► Controller Pod (C++) ──TCP──► Simulator Pod (Python) 
                                  │
                            SECS/GEM Pod ──TCP──► Mock MES Pod
```

---

## AKS Cluster Architecture

### Node pools

```bash
# Create resource group (eastasia — same region as hsunacr for fast image pulls)
az group create --name rg-cvd-sim --location eastasia

# ACR: using existing hsunacr (in resource group 'hsun', eastasia, Premium)
# No need to create — just attach to AKS below

# Create AKS cluster with x86 system pool
az aks create \
  --resource-group rg-cvd-sim \
  --name cvd-cluster \
  --node-count 1 \
  --node-vm-size Standard_D2s_v6 \
  --generate-ssh-keys

# Attach ACR to AKS (allows nodes to pull images without imagePullSecrets)
az aks update \
  --resource-group rg-cvd-sim \
  --name cvd-cluster \
  --attach-acr hsunacr

# Add ARM node pool (Cobalt 100 — real ARM64 hardware)
az aks nodepool add \
  --resource-group rg-cvd-sim \
  --cluster-name cvd-cluster \
  --name armpool \
  --node-vm-size Standard_D2ps_v6 \
  --node-count 1 \
  --labels arch=arm64

# Add x86 node pool (for controller + simulator)
az aks nodepool add \
  --resource-group rg-cvd-sim \
  --cluster-name cvd-cluster \
  --name x86pool \
  --node-vm-size Standard_D2s_v6 \
  --node-count 1 \
  --labels arch=amd64
```

### Why two architectures?

| Node | Architecture | Purpose |
|---|---|---|
| ARM pool | AArch64 (Cobalt 100) | Simulates an ARM-based subsystem controller (gas box, RF match) |
| x86 pool | AMD64 | Simulates the main chamber controller (Beckhoff IPC equivalent) |

This lets you validate **cross-compilation** — build ARM and x86 binaries from the same codebase, deploy each to the correct node pool, and verify they communicate correctly.

---

## Pod Layout

```
┌─ AKS Cluster ────────────────────────────────────────────────────────────┐
│                                                                           │
│  Namespace: cvd-system                                                    │
│                                                                           │
│  x86 node pool                              ARM node pool                 │
│  ┌──────────────────────────┐               ┌─────────────────────────┐  │
│  │ Pod: chamber-controller  │  TCP :5555    │ Pod: gasbox-controller  │  │
│  │ C++ control loop         │◄─────────────►│ C++ gas subsystem       │  │
│  │ (main state machine,     │               │ (MFC PID loops,         │  │
│  │  recipe engine, thermal  │               │  gas interlock logic)   │  │
│  │  PID, pressure PID)      │               │                         │  │
│  │                           │               │ ARM64 binary            │  │
│  │ AMD64 binary              │               │ nodeSelector: arm64     │  │
│  │ nodeSelector: amd64       │               └─────────────────────────┘  │
│  └─────────────┬────────────┘                                             │
│                │                                                          │
│  ┌─────────────┴────────────┐               ┌─────────────────────────┐  │
│  │ Pod: chamber-simulator   │               │ Pod: gasbox-simulator   │  │
│  │ Python physics model     │               │ Python MFC + gas model  │  │
│  │ (thermal, pressure,      │               │ (flow dynamics,          │  │
│  │  plasma response)        │               │  mixing, depletion)     │  │
│  │ TCP :5555                │               │ TCP :5556               │  │
│  └──────────────────────────┘               └─────────────────────────┘  │
│                                                                           │
│  ┌──────────────────────────┐               ┌─────────────────────────┐  │
│  │ Pod: hmi-frontend        │               │ Pod: secs-gem-host      │  │
│  │ React + nginx            │               │ Mock fab MES            │  │
│  │ WebSocket → controller   │               │ HSMS TCP :5000          │  │
│  │ Service: LoadBalancer    │               │ Validates E30/E37/E87   │  │
│  │ :443                     │               │ Service: ClusterIP      │  │
│  └──────────────────────────┘               └─────────────────────────┘  │
│                                                                           │
│  ┌──────────────────────────────────────────────────────────────────────┐ │
│  │ Pod: data-logger                                                     │ │
│  │ Collects telemetry from controller pods → writes to InfluxDB /       │ │
│  │ Azure Monitor                                                        │ │
│  │ Pod: grafana — dashboards for process visualization                  │ │
│  └──────────────────────────────────────────────────────────────────────┘ │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## Kubernetes Manifests

### Namespace

```yaml
apiVersion: v1
kind: Namespace
metadata:
  name: cvd-system
```

### Chamber Controller (x86)

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: chamber-controller
  namespace: cvd-system
spec:
  replicas: 1
  selector:
    matchLabels:
      app: chamber-controller
  template:
    metadata:
      labels:
        app: chamber-controller
    spec:
      nodeSelector:
        arch: amd64
      containers:
      - name: controller
        image: hsunacr.azurecr.io/chamber-controller:latest   # x86 image
        ports:
        - containerPort: 8080    # HMI WebSocket
        - containerPort: 5000    # SECS/GEM HSMS
        env:
        - name: SIM_HOST
          value: "chamber-simulator.cvd-system.svc.cluster.local"
        - name: SIM_PORT
          value: "5555"
        - name: GASBOX_HOST
          value: "gasbox-controller.cvd-system.svc.cluster.local"
        - name: GASBOX_PORT
          value: "5556"
        resources:
          requests:
            cpu: "2"
            memory: "2Gi"
          limits:
            cpu: "3"
            memory: "4Gi"
---
apiVersion: v1
kind: Service
metadata:
  name: chamber-controller
  namespace: cvd-system
spec:
  selector:
    app: chamber-controller
  ports:
  - name: hmi
    port: 8080
  - name: secsgem
    port: 5000
```

### Gas Box Controller (ARM)

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: gasbox-controller
  namespace: cvd-system
spec:
  replicas: 1
  selector:
    matchLabels:
      app: gasbox-controller
  template:
    metadata:
      labels:
        app: gasbox-controller
    spec:
      nodeSelector:
        arch: arm64
      containers:
      - name: controller
        image: hsunacr.azurecr.io/gasbox-controller:latest   # ARM64 image
        ports:
        - containerPort: 5556
        env:
        - name: SIM_HOST
          value: "gasbox-simulator.cvd-system.svc.cluster.local"
        - name: SIM_PORT
          value: "5557"
        resources:
          requests:
            cpu: "1"
            memory: "512Mi"
---
apiVersion: v1
kind: Service
metadata:
  name: gasbox-controller
  namespace: cvd-system
spec:
  selector:
    app: gasbox-controller
  ports:
  - port: 5556
```

### Chamber Physics Simulator

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: chamber-simulator
  namespace: cvd-system
spec:
  replicas: 1
  selector:
    matchLabels:
      app: chamber-simulator
  template:
    metadata:
      labels:
        app: chamber-simulator
    spec:
      containers:
      - name: simulator
        image: hsunacr.azurecr.io/chamber-simulator:latest
        ports:
        - containerPort: 5555
---
apiVersion: v1
kind: Service
metadata:
  name: chamber-simulator
  namespace: cvd-system
spec:
  selector:
    app: chamber-simulator
  ports:
  - port: 5555
```

### HMI Frontend

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: hmi-frontend
  namespace: cvd-system
spec:
  replicas: 1
  selector:
    matchLabels:
      app: hmi-frontend
  template:
    metadata:
      labels:
        app: hmi-frontend
    spec:
      containers:
      - name: hmi
        image: hsunacr.azurecr.io/hmi-frontend:latest
        ports:
        - containerPort: 80
---
apiVersion: v1
kind: Service
metadata:
  name: hmi-frontend
  namespace: cvd-system
spec:
  type: LoadBalancer
  selector:
    app: hmi-frontend
  ports:
  - port: 443
    targetPort: 80
```

---

## Multi-Architecture Build Pipeline

Build both x86 and ARM images from the same source using `docker buildx`:

```yaml
# .github/workflows/build.yml
name: Build & Push Multi-Arch

on:
  push:
    branches: [main]

jobs:
  build-controller:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v4

    - name: Set up QEMU (for ARM cross-build)
      uses: docker/setup-qemu-action@v3

    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v3

    - name: Login to ACR
      uses: azure/docker-login@v2
      with:
        login-server: hsunacr.azurecr.io
        username: ${{ secrets.ACR_USERNAME }}
        password: ${{ secrets.ACR_PASSWORD }}

    - name: Build & push chamber-controller (x86)
      run: |
        docker buildx build \
          --platform linux/amd64 \
          --push \
          -t hsunacr.azurecr.io/chamber-controller:latest \
          -f docker/Dockerfile.controller .

    - name: Build & push gasbox-controller (ARM64)
      run: |
        docker buildx build \
          --platform linux/arm64 \
          --push \
          -t hsunacr.azurecr.io/gasbox-controller:latest \
          -f docker/Dockerfile.controller .
```

### Dockerfile (shared for both architectures)

```dockerfile
# docker/Dockerfile.controller
FROM ubuntu:22.04 AS build

RUN apt-get update && apt-get install -y \
    build-essential cmake libgtest-dev nlohmann-json3-dev

WORKDIR /src
COPY . .
RUN cmake -B build -DUSE_SIM=ON && cmake --build build

FROM ubuntu:22.04
COPY --from=build /src/build/cvd_controller /usr/local/bin/
CMD ["cvd_controller"]
```

Docker buildx + QEMU handles the cross-compilation transparently — same Dockerfile produces x86 or ARM binary depending on `--platform`.

---

## Inter-Pod Communication Pattern

```
Real EtherCAT:
  Controller ──raw L2 frames (250μs cycle)──► I/O slave

AKS simulation:
  Controller Pod ──TCP socket (best-effort)──► Simulator Pod
```

### Protocol (simple binary over TCP)

```cpp
// Shared between controller and simulator
#pragma pack(push, 1)
struct SimPacket {
    uint32_t seq;           // sequence number
    double   timestamp;     // simulated time
    // Sensor readings (simulator → controller)
    double   temperature;   // °C
    double   pressure;      // Torr
    double   rf_forward;    // W
    double   rf_reflected;  // W
    double   gas_flows[8];  // sccm
    // Actuator commands (controller → simulator)
    double   heater_power;  // 0-100%
    double   throttle_pos;  // 0-100%
    double   rf_setpoint;   // W
    double   mfc_setpoints[8]; // sccm
};
#pragma pack(pop)
```

Each cycle:
1. Controller sends actuator commands to simulator
2. Simulator advances physics model by Δt, sends back sensor readings
3. Controller runs control loop on new readings

---

## What This Architecture Validates

| Component | Validatable in AKS? | How |
|---|---|---|
| **C++ control logic** (state machine, recipe engine) | Fully | Runs in pods, same binary as real target |
| **Cross-compilation** (x86 + ARM from same source) | Fully | Buildx produces both; each runs on correct node pool |
| **Inter-controller communication** | Fully | TCP between pods mimics inter-controller Ethernet |
| **SECS/GEM protocol** | Fully | Mock MES pod validates message sequences |
| **HMI** | Fully | React frontend connects via WebSocket |
| **PID tuning** (against physics model) | Mostly | Physics model fidelity limits accuracy |
| **Recipe sequencing** | Fully | State machine transitions, step timing, parameter ramps |
| **Safety interlocks** (software) | Fully | Interlock logic runs identically |
| **Data logging / dashboards** | Fully | Grafana + InfluxDB in-cluster |
| **CI/CD pipeline** | Fully | GitHub Actions → ACR → AKS rolling update |

---

## Limitations

### Hard limitations (cannot work around in AKS)

| Limitation | Why | Impact |
|---|---|---|
| **No real-time scheduling** | AKS nodes run standard Linux kernel. No `SCHED_FIFO`, no CPU isolation, no PREEMPT_RT. K8s scheduler can preempt or migrate pods. | Control loop jitter will be **milliseconds**, not microseconds. Cannot validate timing-critical behavior. |
| **No EtherCAT** | EtherCAT requires raw L2 Ethernet frames (EtherType 0x88A4). AKS pod networking is an overlay (VXLAN/Geneve) that only passes IP traffic. | Must replace EtherCAT with TCP simulation. Cannot test actual fieldbus protocol, frame timing, or distributed clocks. |
| **No hardware watchdog** | No physical watchdog timer IC to kick. | Cannot test watchdog timeout → hardware reset safety path. |
| **No hardware safety interlocks** | No physical relays, no safety PLC (TwinSAFE). | Can only test software interlocks. Hardware failsafe paths untestable. |
| **No TwinCAT runtime** | TwinCAT is proprietary, licensed per-target, x86-only, and requires its own kernel-level real-time scheduler. Cannot run in a container. | Must rewrite PLC logic in C++ for the simulation. Cannot test actual Structured Text code. |
| **No ADS protocol** | ADS is a TwinCAT-native protocol. Without TwinCAT, there's no ADS server. | HMI must use WebSocket/REST instead. Cannot validate real ADS communication. |

### Soft limitations (can partially work around)

| Limitation | Workaround | Remaining gap |
|---|---|---|
| **Pod-to-pod latency** (~0.1–1 ms, non-deterministic) | Use `hostNetwork: true` to reduce overlay overhead. Co-locate pods on same node with pod affinity. | Still not deterministic; jitter depends on node load. |
| **No RT kernel features** | Set pod CPU requests/limits to get dedicated cores. Use `isolcpus` on node (requires custom node image). | Helps, but not true RT — no priority inheritance, no deterministic scheduling. |
| **Physics model fidelity** | Use more detailed models (plasma kinetics, thermal FEA). Run model on GPU node pool for speed. | Model is only as good as your process knowledge. Real plasma is chaotic. |
| **No analog signal noise** | Add simulated noise to sensor readings in the simulator. | Approximation — real sensor noise has frequency-dependent characteristics. |

### What you'd need to validate "for real" (outside AKS)

| Test | Where to do it |
|---|---|
| RT loop jitter < 100 μs | Dedicated VM with PREEMPT_RT kernel (not AKS) or bare-metal |
| EtherCAT frame timing | Physical PC + Beckhoff I/O terminals (~$500 starter kit) |
| TwinCAT PLC code (Structured Text) | TwinCAT XAE simulator on a Windows PC (free for simulation) |
| Hardware safety interlocks | Physical relay board + safety PLC |
| Full system integration | Real controller + real I/O + process chamber |

---

## Cost Estimate

| Resource | Spec | ~Monthly (pay-as-you-go) |
|---|---|---|
| AKS cluster (no management fee) | Free | $0 |
| x86 node pool | 1× Standard_D2s_v6 (2 vCPU, 8 GB) | ~$70 |
| ARM node pool | 1× Standard_D2ps_v6 (Cobalt 100, 2 vCPU, 8 GB) | ~$55 |
| ACR (Basic tier) | Container image storage | ~$5 |
| Load Balancer | Standard LB for HMI | ~$18 |
| **Total** | | **~$148/month** |

**Cost reduction:**
- Use spot node pools → ~60% savings → **~$60/month**
- Deallocate when not developing (`az aks stop`) → pay only when active
- v6 Cobalt 100 ARM nodes are already ~20% cheaper than v5 Ampere Altra

---

## Summary Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│   What AKS validates                   What requires real hardware   │
│   ─────────────────                    ──────────────────────────    │
│                                                                      │
│   ✓ Control logic correctness          ✗ RT loop timing (< 100μs)  │
│   ✓ State machine transitions          ✗ EtherCAT protocol          │
│   ✓ Recipe sequencing                  ✗ Hardware safety interlocks │
│   ✓ PID behavior (against model)       ✗ TwinCAT Structured Text   │
│   ✓ SECS/GEM protocol                 ✗ ADS protocol               │
│   ✓ HMI functionality                 ✗ Analog signal integrity    │
│   ✓ Cross-arch builds (x86 + ARM)     ✗ Hardware watchdog          │
│   ✓ Inter-controller communication                                  │
│   ✓ CI/CD pipeline                                                  │
│   ✓ Data logging / visualization                                    │
│                                                                      │
│   ~70% of the software stack              ~30% needs physical HW    │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘
```

AKS is excellent for developing and testing the **software architecture** — the control logic, communication protocols, HMI, and CI/CD. The remaining 30% (real-time determinism, fieldbus protocols, hardware safety) requires physical hardware, even if it's just a $500 Beckhoff starter kit on your desk.
